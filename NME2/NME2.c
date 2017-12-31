/**
    Reference links:
    https://steamcommunity.com/app/524220/discussions/0/135511913383645482/
    https://github.com/hcs64/ww2ogg

    Textures/Audio:
    http://forum.xentax.com/viewtopic.php?f=10&t=16011&start=0


    Video (*.usm):
    Video copy:
    ffmpeg -f mpegvideo -i <in> -c copy out.mpeg (Not very valid, does play in VLC)

*/
#include <SDKDDKVer.h>

#include "defs.h"
#include "utils.h"
#include "wwriff.h"
#include "bitmanip.h"

// Parses arguments for video files
void ParseVideoArgs(char* video_codec_opt, char* video_quality_opt, char* video_filter_opt, File* file, bool verbose);

// Parses arguments for audio files
void ParseAudioArgs(int argc, char* argv[], File* file, bool wsp);

int main(int argc, char* argv[]) {
    VersionInfo version_info = PrintVersionInfo();
    UNUSED(version_info);

    // Check if ffmpeg is available
    if (system("where ffmpeg > nul 2>&1") != 0) {
        perrf("Exiting: ffmpeg not found");

        return 1;
    } else {
        puts("ffmpeg is available");
    }

    // Exits if no arguments were provided to the program
    if (argc < 2) {
        perrf("Exiting: no arguments provided");
        return 1;
    }

    // Try to resolve the first argument as a path
    char* input_path_full = malloc(_MAX_PATH);
    path_t input_file_type;

    if ((input_file_type = ResolveFullpath(input_path_full, argv[1])) == PATH_INV) {
        perrf("Path '%s' is invalid", argv[1]);

        return 1;
    } else if (input_file_type == PATH_DIR) {
        printf("Path '%s' points to a directory\n", argv[1]);
    } else if (input_file_type == PATH_FILE) {
        printf("Path '%s' points to a file\n", argv[1]);
    }

    // Parse arguments
    char* video_codec_opt   = NULL;
    char* video_quality_opt = NULL;
    char* video_filter_opt  = NULL;
    char* audio_codec_opt   = NULL;
    char* audio_quality_opt = NULL;
    char* pattern_opt       = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-vc") == 0) {
            if (i + 1 >= argc) {
                perrf("-vc needs a value\n");

                return 1;
            }

            video_codec_opt = argv[++i];
        } else if (strcmp(argv[i], "-vq") == 0) {
            if (i + 1 >= argc) {
                perrf("-vq needs a value\n");
                
                return 1;
            }
            
            video_quality_opt = argv[++i];
        } else if (strcmp(argv[i], "-vf") == 0) {
            if (i + 1 >= argc) {
                perrf("-vf needs a value\n");

                return 1;
            }

            video_filter_opt = argv[++i];
        } else if (strcmp(argv[i], "-ac") == 0) {
            if (i + 1 >= argc) {
                perrf("-ac needs a value\n");

                return 1;
            }

            audio_codec_opt = argv[++i];
        } else if (strcmp(argv[i], "-aq") == 0) {
            if (i + 1 >= argc) {
                perrf("-aq needs a value\n");

                return 1;
            }

            audio_quality_opt = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                perrf("-p needs a value\n");

                return 1;
            }

            pattern_opt = argv[++i];
        } else {
            perrf("Unknown option '%s'\n", argv[i]);

            return 1;
        }
    }

    // Split input path into it's components
    fpath input_path;
    errno_t splitpath_err;
    if ((splitpath_err = _splitpath_s(input_path_full, input_path.drive, _MAX_DRIVE, input_path.dir, _MAX_DIR, input_path.fname, _MAX_FNAME, input_path.ext, _MAX_EXT)) != 0) {
        perrf("Could not split path, error %i\n", splitpath_err);

        return 1;
    }

    // If the input path is a directory we want to have a search pattern at the end of the path
    if (input_file_type == PATH_DIR) {
        if (!pattern_opt) {
            pwarnf("Pattern not specified, using fallback *.*\n");

            strcpy_s(input_path.fname, _MAX_FNAME, "*.*");
        } else {
            printf("Got file pattern '%s'\n", pattern_opt);

            strcpy_s(input_path.fname, _MAX_FNAME, pattern_opt);
        }
    }

    wchar_t* input_path_w = MakePathW(input_path);
    WIN32_FIND_DATA find_data;
    HANDLE h_find;

    // Continue if a file is found, display error message if not
    if ((h_find = FindFirstFile(input_path_w, &find_data)) != INVALID_HANDLE_VALUE) {
        // Count files to allocate enough memory
        int n = 0;
        do {
            n++;
        } while (FindNextFile(h_find, &find_data) != 0);

        File* files = calloc(n, sizeof(File));

        // Loop through all files
        bool overwrite_all = false;
        FindClose(h_find);
        h_find = FindFirstFile(input_path_w, &find_data);
        int n_files = 0;
        do {
            char* file_name = malloc(_MAX_FNAME + _MAX_EXT);
            char* current_file_path = malloc(_MAX_PATH);
            File current_file;
            fpath input_base_path;
            strcpy_s(input_base_path.drive, _MAX_DRIVE, input_path.drive);
            strcpy_s(input_base_path.dir, _MAX_DIR, input_path.dir);

            wcstombs_s(NULL, file_name, _MAX_FNAME + _MAX_EXT, find_data.cFileName, _MAX_FNAME + _MAX_EXT);

            strcpy_s(input_base_path.fname, _MAX_FNAME, file_name);

            strcpy_s(current_file_path, _MAX_PATH, MakePath(input_base_path));

            free(file_name);

            errno_t splitpath_err;
            if ((splitpath_err = _splitpath_s(current_file_path, current_file.input.drive, _MAX_DRIVE, current_file.input.dir, _MAX_DIR, current_file.input.fname, _MAX_FNAME, current_file.input.ext, _MAX_EXT)) != 0) {
                perrf("Error splitting path for '%s', code '%i'\n", current_file_path, splitpath_err);

                continue;
            }

            free(current_file_path);

            // Filters out "." and ".."
            if (strcmp(current_file.input.ext, ".") == 0) {
                free(file_name);
                free(current_file_path);

                continue;
            }

            current_file.format = GetFileFormat(current_file.input);
#if 0
            if (!overwrite_all) {
                DWORD attr = GetFileAttributes(MakePathW(current_file.output));
                yn_response response = RESPONSE_NIL;

                if ((attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) && (response = ConfirmOverwrite(current_file.output, true)) != RESPONSE_YES) {
                    if (response == RESPONSE_YES_ALL) {
                        overwrite_all = true;
                    } else {
                        perrf("Not overwriting file '%s%s'\n", current_file.output.fname, current_file.output.ext);

                        continue;
                    }
                }
            }
#endif

            files[n_files] = current_file;
            n_files++;
        } while (FindNextFile(h_find, &find_data) != 0);

        FindClose(h_find);

        uint32_t success = 0;
        uint32_t failure = 0;
        for (int i = 0; i < n_files; i++) {
            switch(files[i].format) {
                case FORMAT_USM: {
                        ParseVideoArgs(video_codec_opt, video_quality_opt, video_filter_opt, &files[i], i == 0);
                        char* cmd = ConstructCommand(&files[i]);

                        WriteToLog(cmd);

                        printf("\nStarting conversion %i of %i\n\n", i + 1, n_files);

                        int ffmpeg = system(cmd);

                        free(cmd);

                        if (ffmpeg != 0) {
                            perrf("\nConversion %i failed with status code %i\n", i + 1, ffmpeg);
                            failure++;
                        } else {
                            printf("\nConversion %i succesful\n", i + 1);
                            success++;
                        }

                        char* finished_msg = malloc(37);

                        sprintf_s(finished_msg, 37, "Conversion finished with exit code %i", ffmpeg);

                        finished_msg = TRIM(finished_msg);

                        WriteToLog(finished_msg);

                        free(finished_msg);

                        break;
                    }

                default:
                    perrf("Unknown format %i for '%s'\n", files[i].format, MakePath(files[i].input));

                    failure++;
            }
        }

        free(files);

        printf("\nConverted %i files. Success: %u, failures: %u\n", n_files, success, failure);
    } else {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            perrf("No files found");
        } else {
            perrf("Invalid pattern");
        }

        return 1;
    }

    return 0;
}

void ParseVideoArgs(char* video_codec_opt, char* video_quality_opt, char* video_filter_opt, File* file, bool verbose) {
    // Converts the -vc argument to the appropriate ffmpeg encoder
    if (video_codec_opt) {
        if (_stricmp(video_codec_opt, "vp9") == 0) {
            file->args.video_args.encoder = VP9_LIB;
        } else if (_stricmp(video_codec_opt, "h265") == 0 || _stricmp(video_codec_opt, "hevc") == 0) {
            file->args.video_args.encoder = H265_LIB;
        } else if (_stricmp(video_codec_opt, "h264") == 0) {
            file->args.video_args.encoder = H264_LIB;
        } else if (_stricmp(video_codec_opt, "copy") == 0) {
            file->args.video_args.encoder = "copy";
        } else {
            perrf("Unknown video codec '%s'", video_codec_opt);

            exit(1);
        }
    } else {
        if (verbose) {
            pwarnf("Video codec not specified, using fallback '%s'\n", VIDEO_LIB_FALLBACK);
        }

        file->args.video_args.encoder = VIDEO_LIB_FALLBACK;
    }

    // Converts the -vq argument to the appropriate ffmpeg argument
    char* quality = malloc(12);
    
    if (video_quality_opt) {
        if (strcmp(file->args.video_args.encoder, "copy") == 0) {
            pwarnf("Dropping quality argument '%s' in favour of '-c copy'\n", video_quality_opt);

            sprintf_s(quality, 12, "%s", "");
        } else if (_stricmp(video_quality_opt, "lossless") == 0) {
            if (strcmp(file->args.video_args.encoder, VP9_LIB) == 0) {
                strcpy_s(quality, 12, "-lossless 1");
            } else {
                strcpy_s(quality, 12, "-crf 0");
            }
        } else if (isdigit(video_quality_opt[0])) {
            char quality_suffix = video_quality_opt[strlen(video_quality_opt) - 1];

            if (quality_suffix == 'k' || quality_suffix == 'M') {
                sprintf_s(quality, 12, "-b:v %s", video_quality_opt);
            } else {
                // Validate the crf value on a per-lib basis
                int crf = atoi(video_quality_opt);

                // libvpx-vp9 supports crf values 0-63
                // libx264 and libx265 support 0-51
                if (!(strcmp(file->args.video_args.encoder, VP9_LIB) == 0 && 0 <= crf <= 63) &&
                    !((strcmp(file->args.video_args.encoder, H264_LIB) == 0 || strcmp(file->args.video_args.encoder, H265_LIB) == 0) && 0 <= crf <= 51)) {
                    perrf("CRF value '%i' is not supported for encoder '%s'", crf, file->args.video_args.encoder);

                    exit(1);
                } else if (strcmp(file->args.video_args.encoder, VP9_LIB) == 0) {
                    quality = realloc(quality, 19);

                    sprintf_s(quality, 19, "-crf %s -b:v 0", video_quality_opt);
                } else {
                    sprintf_s(quality, 12, "-crf %s", video_quality_opt);
                }
            }
        } else {
            perrf("Unknown quality format '%s'", video_quality_opt);

            exit(1);
        }
    } else {
        char* fallback_quality = malloc(LongestStrlen(3, VIDEO_QUALITY_FALLBACK_VP9, VIDEO_QUALITY_FALLBACK_H265, VIDEO_QUALITY_FALLBACK_H264));

        int quality_size = 12;

        if (strcmp(file->args.video_args.encoder, VP9_LIB) == 0) {
            fallback_quality = VIDEO_QUALITY_FALLBACK_VP9;

            quality_size = 15;

            quality = realloc(quality, quality_size);
        } else if (strcmp(file->args.video_args.encoder, H265_LIB) == 0) {
            fallback_quality = VIDEO_QUALITY_FALLBACK_H265;
        } else if (strcmp(file->args.video_args.encoder, H264_LIB) == 0) {
            fallback_quality = VIDEO_QUALITY_FALLBACK_H264;
        } else if (strcmp(file->args.video_args.encoder, "copy") == 0) {
            fallback_quality = "";
        }

        if (verbose) {
            pwarnf("Video quality not specified, using fallback '%s'\n", fallback_quality);
        }

        sprintf_s(quality, quality_size, "%s", fallback_quality);
    }

    file->args.video_args.quality = quality;

    // If the -vf arguments was omitted we set it to crop=1600:900 to crop out the 4 empty pixel rows at the bottom of each frame
    if (video_filter_opt) {
        // If the -vf arguments contains the crop filter we use the entire argument as filter, else we prepend the default filter
        if (strcmp(file->args.video_args.encoder, "copy") == 0) {
            pwarnf("Dropping filters '%s' in favour of '-c copy'\n", file->args.video_args.filters);

            file->args.video_args.filters = "";
        } else if (strstr(video_filter_opt, "crop=") != NULL) {
            char* filter = malloc(5 + strlen(video_filter_opt));

            sprintf_s(filter, 5 + strlen(video_filter_opt), "-vf %s", video_filter_opt);

            file->args.video_args.filters = filter;
        } else {
            char* filter = malloc(23 + strlen(video_filter_opt));

            sprintf_s(filter, 23 + strlen(video_filter_opt), "-vf crop=1600:900:0:0,%s", video_filter_opt);

            file->args.video_args.filters = filter;
        }
    } else {
        if (verbose) {
            puts("Filters not specified, using fallback 'crop=1600:900:0:0'");
        }

        file->args.video_args.filters = "-vf crop=1600:900:0:0";
    }

    // Uses a good format and container for the output file, but keeps the original folder and base name
    strcpy_s(file->output.drive, _MAX_DRIVE, file->input.drive);
    strcpy_s(file->output.dir, _MAX_DIR, file->input.dir);
    strcpy_s(file->output.fname, _MAX_FNAME, file->input.fname);
    if (strcmp(file->args.video_args.encoder, VP9_LIB) == 0) {
        file->args.video_args.format = "-f webm";

        strcpy_s(file->output.ext, _MAX_EXT, ".webm");
    } else if (strcmp(file->args.video_args.encoder, H265_LIB) == 0) {
        file->args.video_args.format = "-f matroska";

        strcpy_s(file->output.ext, _MAX_EXT, ".mkv");
    } else if (strcmp(file->args.video_args.encoder, H264_LIB) == 0) {
        file->args.video_args.format = "-f mp4";

        strcpy_s(file->output.ext, _MAX_EXT, ".mp4");
    } else if (strcmp(file->args.video_args.encoder, "copy") == 0) {
        file->args.video_args.format = "";

        strcpy_s(file->output.ext, _MAX_EXT, ".mpeg");
    }
}

void ParseAudioArgs(int argc, char* argv[], File* file, bool wsp) {
    if (wsp) {
        strcpy_s(file->output.drive, _MAX_DRIVE, file->input.drive);
        strcpy_s(file->output.dir, _MAX_DIR, file->input.dir);

        // Read the entire input file into memory
        FILE* infile;
        fopen_s(&infile, MakePath(file->input), "rb");

        fseek(infile, 0, SEEK_END);
        long file_size = ftell(infile);
        fseek(infile, 0, SEEK_SET);

        char* data = malloc(file_size);
        size_t read = fread_s(data, file_size, 1, file_size, infile);
        
        fclose(infile);

        // Shouldn't happen, could happen
        if (read < 4) {
            perrf("Error reading file %s", MakePath(file->input));

            exit(1);
        }

        // If the first RIFF doesn't start at index 0
        if (split_bytes(data, file_size, "RIFF", 4, 0) != 0) {
            perrf("Invalid file %i", MakePath(file->input));

            exit(1);
        }

        // Count the occurences of the RIFF header
        bool end_reached = false;
        uint64_t start = 0;
        uint64_t count = 0;
        while (!end_reached) {
            uint64_t end = split_bytes(data, file_size, "RIFF", 4, start + 1);
            
            if (end == -1) {
                end_reached = true;
            }

            start = end + 1;
            count++;
        }

        // Save each embeded file individually
        char** files = malloc(count * sizeof(char*));
        uint64_t* sizes = calloc(count, sizeof(uint64_t));
        start = 0;
        for (uint64_t i = 0; i < count; i++) {
            uint64_t end = split_bytes(data, file_size, "RIFF", 4, start + 1);

            if (end == -1) {
                end = file_size;
            }

            char* f = malloc(end - start);
            memcpy_s(f, end - start, &data[start], end - start);

            files[i] = f;
            sizes[i] = end - start;

            start = end + 1;
        }

        free(data);

        for (uint64_t i = 0; i < count; i++) {
            char* c = malloc(CMD_MAX_LENGTH);
            sprintf_s(c, CMD_MAX_LENGTH, "ffmpeg -hide_banner -v fatal -stats -i - -y tmp/%lli.flac", i);

            FILE* ffmpeg = _popen(c, "wb");

            membuf buf;
            buf.data = files[i];
            buf.size = sizes[i];
            buf.pos = 0;

            errno_t err = create_ogg(&buf, ffmpeg);

            _pclose(ffmpeg);

            free(files[i]);

            printf("Conversion %lli finished with code %i\n", i, err);
        }

        free(files);
        free(sizes);
    }
}