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

// Parses arguments for video files
void ParseVideoArgs(int argc, char* argv[], File* file, bool verbose);

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

    if (input_file_type == PATH_FILE) {
        File* file = malloc(sizeof(File));

        errno_t splitpath_err;
        if ((splitpath_err = _splitpath_s(input_path_full, file->input.drive, _MAX_DRIVE, file->input.dir, _MAX_DIR, file->input.fname, _MAX_FNAME, file->input.ext, _MAX_EXT)) != 0) {
            perrf("Could not split path, error %i", splitpath_err);

            return 1;
        }

        free(input_path_full);

        file->format = GetFileFormat(file->input);

        switch(file->format){
            case FORMAT_USM:{
                file->format = FORMAT_USM;
                ParseVideoArgs(argc, argv, file, true);
                PrintSettingsVideo(file);

                break;
            }
            case FORMAT_NIL:
            default:
                perrf("Unsupported format '%s'", file->input.ext);

                return 1;
        }

        DWORD attr = GetFileAttributes(MakePathW(file->output));

        if ((attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) && ConfirmOverwrite(file->output, false) != RESPONSE_YES) {
            perrf("Not overwriting file, exiting");

            return 1;
        }

        char* cmd = ConstructCommand(file);

        WriteToLog(cmd);

        puts("Starting conversion\n");

        int ffmpeg = system(cmd);

        if (ffmpeg != 0) {
            perrf("\nConversion failed with status code %i", ffmpeg);
        } else {
            puts("\nConversion succesful");
        }

        char* finished_msg = malloc(37);

        sprintf_s(finished_msg, 37, "Conversion finished with exit code %i", ffmpeg);

        finished_msg = TRIM(finished_msg);

        WriteToLog(finished_msg);

        free(finished_msg);
    } else if (input_file_type == PATH_DIR) {
        char* pattern = GetOption("-p", argc, argv);

        // If there was no pattern provided we search for everything
        if (pattern == NULL) {
            pwarnf("No file pattern specified, falling back to default '*.*'\n");

            pattern = "*";
        } else {
            printf("Got file pattern '%s'\n", pattern);
        }

        //printf()

        size_t pattern_length = strlen(pattern) * sizeof(wchar_t);
        size_t input_path_size = (strlen(input_path_full) + strlen(pattern)) * sizeof(wchar_t);

        wchar_t* pattern_w = malloc(pattern_length);
        wchar_t* input_path_w = malloc(input_path_size);

        mbstowcs_s(NULL, pattern_w, pattern_length, pattern, _TRUNCATE);
        mbstowcs_s(NULL, input_path_w, input_path_size, input_path_full, _TRUNCATE);

        wcscat_s(input_path_w, input_path_size, pattern_w);

        WIN32_FIND_DATA data;
        HANDLE h_find;

        if ((h_find = FindFirstFile(input_path_w, &data)) != INVALID_HANDLE_VALUE) {
            int n = 0;

            do {
                n++;
            } while (FindNextFile(h_find, &data) != 0);

            File* files = calloc(n, sizeof(File));

            bool overwrite_all = false;

            FindClose(h_find);

            h_find = FindFirstFile(input_path_w, &data);
            
            int n_files = 0;

            do {
                char* file_name = malloc(_MAX_FNAME + _MAX_EXT);
                char* file_path = malloc(_MAX_PATH);
                File* file = malloc(sizeof(File));

                wcstombs_s(NULL, file_name, _MAX_FNAME + _MAX_EXT, data.cFileName, _MAX_FNAME + _MAX_EXT);

                strcpy_s(file_path, _MAX_PATH, input_path_full);
                strcat_s(file_path, _MAX_PATH, file_name);

                errno_t splitpath_err;
                if ((splitpath_err = _splitpath_s(file_path, file->input.drive, _MAX_DRIVE, file->input.dir, _MAX_DIR, file->input.fname, _MAX_FNAME, file->input.ext, _MAX_EXT)) != 0) {
                    perrf("Error splitting path for '%s', code '%i'\n", file_path, splitpath_err);

                    continue;
                }

                // Filters out "." and ".."
                if (strcmp(file->input.ext, ".") == 0) {
                    free(file_name);
                    free(file_path);
                    free(file);

                    continue;
                }

                file->format = GetFileFormat(file->input);

                switch (file->format) {
                    case FORMAT_USM: {
                            file->format = FORMAT_USM;
                            ParseVideoArgs(argc, argv, file, false);

                            break;
                        }
                    case FORMAT_NIL:
                        perrf("Unsupported format '%s'\n", file->input.ext);

                        continue;
                        break;
                }

                if (!overwrite_all) {
                    DWORD attr = GetFileAttributes(MakePathW(file->output));
                    yn_response response = RESPONSE_NIL;

                    if ((attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) && (response = ConfirmOverwrite(file->output, true)) != RESPONSE_YES) {
                        if (response == RESPONSE_YES_ALL) {
                            overwrite_all = true;
                        } else {
                            perrf("Not overwriting file '%s%s'\n", file->output.fname, file->output.ext);

                            continue;
                        }
                    }
                }

                files[n_files] = *file;

                free(file_name);
                free(file_path);
                free(file);
                n_files++;
            } while (FindNextFile(h_find, &data) != 0);

            FindClose(h_find);

            for (int i = 0; i < n_files; i++) {
                char* cmd = ConstructCommand(&files[i]);

                WriteToLog(cmd);

                printf("\nStarting conversion %i of %i\n\n", i + 1, n_files);

                int ffmpeg = system(cmd);

                free(cmd);

                if (ffmpeg != 0) {
                    perrf("\nConversion %i failed with status code %i\n", i + 1, ffmpeg);
                } else {
                    printf("\nConversion %i succesful\n", i + 1);
                }

                char* finished_msg = malloc(37);

                sprintf_s(finished_msg, 37, "Conversion finished with exit code %i", ffmpeg);

                finished_msg = TRIM(finished_msg);

                WriteToLog(finished_msg);

                free(finished_msg);
            }

            free(files);
            pwarnf("done\n");
        } else {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                perrf("No files found");
            } else {
                perrf("Invalid pattern");
            }
            
            return 1;
        }
    }

    return 1;
}

void ParseVideoArgs(int argc, char* argv[], File* file, bool verbose) {
    // The file is a *.usm file
    file->format = FORMAT_USM;

    // Converts the -c argument to the appropriate ffmpeg encoder
    if (!OptionExists("-c", argc, argv)) {
        if (verbose) {
            pwarnf("Video codec not specified, falling back to '%s'\n", VIDEO_LIB_FALLBACK);
        }
        file->args.video_args.encoder = VIDEO_LIB_FALLBACK;
    } else {
        char* codec = GetOption("-c", argc, argv);

        if (_stricmp(codec, "vp9") == 0) {
            file->args.video_args.encoder = VP9_LIB;
        } else if (_stricmp(codec, "h265") == 0 || _stricmp(codec, "hevc") == 0) {
            file->args.video_args.encoder = H265_LIB;
        } else if (_stricmp(codec, "h264") == 0) {
            file->args.video_args.encoder = H264_LIB;
        } else if(_stricmp(codec, "copy") == 0){
            file->args.video_args.encoder = "copy";
        } else {
            perrf("Unknown video codec '%s'", codec);

            exit(1);
        }
    }

    // Converts the -q argument to the appropriate ffmpeg argument

    char* quality = malloc(12);

    if (!OptionExists("-q", argc, argv)) {
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
            pwarnf("Video quality not specified, falling back to '%s'\n", fallback_quality);
        }

        sprintf_s(quality, quality_size, "%s", fallback_quality);
    } else {
        char* qual = GetOption("-q", argc, argv);

        if (strcmp(file->args.video_args.encoder, "copy") == 0) {
            pwarnf("Dropping quality argument '%s' in favour of '-c copy'\n", qual);

            sprintf_s(quality, 12, "%s", "");
        } else if (_stricmp(qual, "lossless") == 0) {
            if (strcmp(file->args.video_args.encoder, VP9_LIB) == 0) {
                strcpy_s(quality, 12, "-lossless 1");
            } else {
                strcpy_s(quality, 12, "-crf 0");
            }
        } else if (isdigit(qual[0])) {
            char quality_suffix = qual[strlen(qual) - 1];

            if (quality_suffix == 'k' || quality_suffix == 'M') {
                sprintf_s(quality, 12, "-b:v %s", qual);
            } else {
                // Validate the crf value on a per-lib basis
                int crf = atoi(qual);

                // libvpx-vp9 supports crf values 0-63
                // libx264 and libx265 support 0-51
                if (!(strcmp(file->args.video_args.encoder, VP9_LIB) == 0 && 0 <= crf <= 63) &&
                    !((strcmp(file->args.video_args.encoder, H264_LIB) == 0 || strcmp(file->args.video_args.encoder, H265_LIB) == 0) && 0 <= crf <= 51)) {
                    perrf("CRF value '%i' is not supported for encoder '%s'", crf, file->args.video_args.encoder);

                    exit(1);
                } else if (strcmp(file->args.video_args.encoder, VP9_LIB) == 0) {
                    quality = realloc(quality, 19);

                    sprintf_s(quality, 19, "-crf %s -b:v 0", qual);
                } else {
                    sprintf_s(quality, 12, "-crf %s", qual);
                }
            }
        } else {
            perrf("Unknown quality '%s'", qual);

            exit(1);
        }
    }

    file->args.video_args.quality = quality;

    // If the -vf arguments was omitted we set it to crop=1600:900 to crop out the 4 empty pixel rows at the bottom of each frame
    if (!OptionExists("-vf", argc, argv)) {
        if (verbose) {
            puts("Filters not specified, falling back to default: 'crop=1600:900:0:0'");
        }

        file->args.video_args.filters = "-vf crop=1600:900:0:0";
    } else {
        char* filters = GetOption("-vf", argc, argv);

        // If the -vf arguments contains the crop filter we use the entire argument as filter, else we prepend the default filter
        if (filters == NULL) {
            perrf("-vf argument has no value");

            exit(1);
        } else if (strstr(filters, "crop=") != NULL) {
            char* filter = malloc(5 + strlen(filters));

            sprintf_s(filter, 5 + strlen(filters), "-vf %s", filters);

            file->args.video_args.filters = filter;
        } else {
            char* filter = malloc(23 + strlen(filters));

            sprintf_s(filter, 23 + strlen(filters), "-vf crop=1600:900:0:0,%s", filters);

            file->args.video_args.filters = filter;
        }
    }

    if (strcmp(file->args.video_args.encoder, "copy") == 0) {
        pwarnf("Dropping filters '%s' in favour of '-c copy'\n", file->args.video_args.filters);

        file->args.video_args.filters = "";
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
