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
#include "defs.h"
#include "utils.h"
#include "wwriff.h"
#include "bitmanip.h"

// Parses arguments for video files
void ParseVideoArgs(char* video_codec_opt, char* video_quality_opt, char* video_filter_opt, File* file, bool verbose);

// Parses arguments for audio files
void ParseAudioArgs(char* audio_codec_opt, char* audio_quality_opt, char* audio_sample_format_opt, File* file, bool verbose);

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
    char* video_codec_opt       = NULL;
    char* video_quality_opt     = NULL;
    char* video_filter_opt      = NULL;
    char* audio_codec_opt       = NULL;
    char* audio_quality_opt     = NULL;
    char* audio_sample_fmt_opt = NULL;
    char* pattern_opt           = NULL;
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
        } else if(strcmp(argv[i], "-sf") == 0) {
            if (i + 1 >= argc) {
                perrf("-sf needs a value\n");

                return 1;
            }

            audio_sample_fmt_opt = argv[++i];
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

                case FORMAT_WSP: {
                        ParseAudioArgs(audio_codec_opt, audio_quality_opt, audio_sample_fmt_opt, &files[i], i == 0);

                        strcpy_s(files[i].output.drive, _MAX_DRIVE, files[i].input.drive);
                        strcpy_s(files[i].output.dir, _MAX_DIR, files[i].input.dir);

                        // Read the entire input file into memory
                        FILE* infile;
                        fopen_s(&infile, MakePath(files[i].input), "rb");

                        fseek(infile, 0, SEEK_END);
                        long file_size = ftell(infile);
                        fseek(infile, 0, SEEK_SET);

                        char* data = malloc(file_size);
                        size_t read = fread(data, file_size, 1, infile);

                        fclose(infile);

                        // Shouldn't happen, could happen
                        if (read < 4) {
                            perrf("Error reading file %s\n", MakePath(files[i].input));

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

                        // Save each embeded file individually in memory
                        char** files_mem = malloc(count * sizeof(char*));
                        uint64_t* sizes = calloc(count, sizeof(uint64_t));
                        start = 0;
                        for (uint64_t j = 0; j < count; j++) {
                            uint64_t end = split_bytes(data, file_size, "RIFF", 4, start + 1);

                            if (end == -1) {
                                end = file_size;
                            }

                            char* f = malloc(end - start);
                            memcpy_s(f, end - start, &data[start], end - start);

                            files_mem[j] = f;
                            sizes[j] = end - start;

                            start = end + 1;
                        }

                        free(data);

                        // Convert all files
                        for (uint64_t j = 0; j < count; j++) {
                            sprintf_s(files[i].output.fname, _MAX_FNAME, "%s_[%lli]", files[i].input.fname, j);
                            char* cmd = ConstructCommand(&files[i]);
                            WriteToLog(cmd);

                            printf("\nStarting conversion %lli of %lli\n\n", j + 1, count);

                            FILE* conversion = _popen(cmd, "wb");

                            free(cmd);

                            membuf buf;
                            buf.data = files_mem[j];
                            buf.size = sizes[j];
                            buf.pos = 0;

                            errno_t err = create_ogg(&buf, conversion);

                            _pclose(conversion);

                            free(files_mem[j]);

                            if (err != 0) {
                                perrf("\nConversion %lli failed with status code %lli\n", j + 1, err);
                                failure++;
                            } else {
                                printf("\nConversion %lli succesful\n", j + 1);
                                success++;
                            }
                        }

                        free(files_mem);
                        free(sizes);
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
            file->args.video_args.encoder = VP9_CODEC;
        } else if (_stricmp(video_codec_opt, "h265") == 0 || _stricmp(video_codec_opt, "hevc") == 0) {
            file->args.video_args.encoder = H265_CODEC;
        } else if (_stricmp(video_codec_opt, "h264") == 0) {
            file->args.video_args.encoder = H264_CODEC;
        } else if (_stricmp(video_codec_opt, "copy") == 0) {
            file->args.video_args.encoder = "copy";
        } else {
            perrf("Unknown video codec '%s'\n", video_codec_opt);

            exit(1);
        }
    } else {
        if (verbose) {
            pwarnf("Video codec not specified, using fallback '%s'\n", VIDEO_CODEC_FALLBACK);
        }

        file->args.video_args.encoder = VIDEO_CODEC_FALLBACK;
    }

    // Converts the -vq argument to the appropriate ffmpeg argument
    char* quality = malloc(12);
    
    if (video_quality_opt) {
        if (strcmp(file->args.video_args.encoder, "copy") == 0) {
            pwarnf("Dropping quality argument '%s' in favour of '-vc copy'\n", video_quality_opt);

            sprintf_s(quality, 12, "%s", "");
        } else if (_stricmp(video_quality_opt, "lossless") == 0) {
            if (strcmp(file->args.video_args.encoder, VP9_CODEC) == 0) {
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
                if (!(strcmp(file->args.video_args.encoder, VP9_CODEC) == 0 && 0 <= crf <= 63) &&
                    !((strcmp(file->args.video_args.encoder, H264_CODEC) == 0 || strcmp(file->args.video_args.encoder, H265_CODEC) == 0) && 0 <= crf <= 51)) {
                    perrf("CRF value '%i' is not supported for encoder '%s'", crf, file->args.video_args.encoder);

                    exit(1);
                } else if (strcmp(file->args.video_args.encoder, VP9_CODEC) == 0) {
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

        if (strcmp(file->args.video_args.encoder, VP9_CODEC) == 0) {
            fallback_quality = VIDEO_QUALITY_FALLBACK_VP9;

            quality_size = 15;

            quality = realloc(quality, quality_size);
        } else if (strcmp(file->args.video_args.encoder, H265_CODEC) == 0) {
            fallback_quality = VIDEO_QUALITY_FALLBACK_H265;
        } else if (strcmp(file->args.video_args.encoder, H264_CODEC) == 0) {
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
    if (strcmp(file->args.video_args.encoder, VP9_CODEC) == 0) {
        file->args.video_args.format = "-f webm";

        strcpy_s(file->output.ext, _MAX_EXT, ".webm");
    } else if (strcmp(file->args.video_args.encoder, H265_CODEC) == 0) {
        file->args.video_args.format = "-f matroska";

        strcpy_s(file->output.ext, _MAX_EXT, ".mkv");
    } else if (strcmp(file->args.video_args.encoder, H264_CODEC) == 0) {
        file->args.video_args.format = "-f mp4";

        strcpy_s(file->output.ext, _MAX_EXT, ".mp4");
    } else if (strcmp(file->args.video_args.encoder, "copy") == 0) {
        file->args.video_args.format = "";

        strcpy_s(file->output.ext, _MAX_EXT, ".mpeg");
    }
}

void ParseAudioArgs(char* audio_codec_opt, char* audio_quality_opt, char* audio_sample_format_opt, File* file, bool verbose) {
    if (audio_codec_opt) {
        if (_stricmp(audio_codec_opt, "flac") == 0) {
            file->args.audio_args.encoder = FLAC_CODEC;
        } else if (_stricmp(audio_codec_opt, "opus") == 0) {
            file->args.audio_args.encoder = OPUS_CODEC;
        } else if (_stricmp(audio_codec_opt, "vorbis") == 0) {
            file->args.audio_args.encoder = VORBIS_CODEC;
        } else if (_stricmp(audio_codec_opt, "aac") == 0) {
            file->args.audio_args.encoder = AAC_CODEC;
        } else if (_stricmp(audio_codec_opt, "mp3") == 0) {
            file->args.audio_args.encoder = MP3_CODEC;
        } else if (_stricmp(audio_codec_opt, "f32") == 0) {
            file->args.audio_args.encoder = PCM_F32_CODEC;
        } else if (_stricmp(audio_codec_opt, "f64") == 0) {
            file->args.audio_args.encoder = PCM_F64_CODEC;
        } else if (_stricmp(audio_codec_opt, "s16") == 0) {
            file->args.audio_args.encoder = PCM_S16_CODEC;
        } else if (_stricmp(audio_codec_opt, "s24") == 0) {
            file->args.audio_args.encoder = PCM_S24_CODEC;
        } else if (_stricmp(audio_codec_opt, "s32") == 0) {
            file->args.audio_args.encoder = PCM_S32_CODEC;
        } else if (_stricmp(audio_codec_opt, "s64") == 0) {
            file->args.audio_args.encoder = PCM_S64_CODEC;
        } else {
            perrf("Unknown audio codec '%s'\n", audio_codec_opt);

            exit(1);
        }
    } else {
        if (verbose) {
            pwarnf("Audio codec not specified, using fallback '%s'\n", AUDIO_CODEC_FALLBACK);
        }

        file->args.audio_args.encoder = AUDIO_CODEC_FALLBACK;
    }


    char* quality = malloc(24);
    if (audio_quality_opt) {
        char quality_suffix = audio_quality_opt[strlen(audio_quality_opt) - 1];
        double q_val = atof(audio_quality_opt);

        if (quality_suffix == 'k') {
            if (strcmp(file->args.audio_args.encoder, VORBIS_CODEC) == 0) {
                // libvorbis supports 45k-500k
                if(45. <= q_val && q_val <= 500.){
                    sprintf_s(quality, 24, "-b:a %s", audio_quality_opt);
                } else if (q_val < 45.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for Vorbis, clamping to 45k\n", audio_quality_opt);
                    }
                    strcpy_s(quality, 24, "-b:a 45k");
                } else if (q_val > 500.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for Vorbis, clamping to 500k\n", audio_quality_opt);
                    }
                    strcpy_s(quality, 24, "-b:a 500k");
                }
            } else if (strcmp(file->args.audio_args.encoder, OPUS_CODEC) == 0) {
                // libopus supports 52k-512k
                if (52. <= q_val && q_val <= 512.) {
                    sprintf_s(quality, 24, "-b:a %s", audio_quality_opt);
                } else if (q_val < 52.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for Opus, clamping to 52k\n", audio_quality_opt);
                    }
                    strcpy_s(quality, 24, "-b:a 45k");
                } else if (q_val > 512.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for Opus, clamping to 512k\n", audio_quality_opt);
                    }
                    strcpy_s(quality, 24, "-b:a 512k");
                }
            } else if (strcmp(file->args.audio_args.encoder, AAC_CODEC) == 0) {
                // aac supports 46k-576k
                if (46. <= q_val && q_val <= 576.) {
                    sprintf_s(quality, 24, "-b:a %s", audio_quality_opt);
                } else if (q_val < 46.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for AAC, clamping to 46k\n", audio_quality_opt);
                    }
                    strcpy_s(quality, 24, "-b:a 45k");
                } else if (q_val > 576.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for AAC, clamping to 576k\n", audio_quality_opt);
                    }
                    strcpy_s(quality, 24, "-b:a 576k");
                }
            } else if (strcmp(file->args.audio_args.encoder, MP3_CODEC) == 0) {
                // libmp3lame supports 8k, 16k, 24k, 32k, 40k, 48k, 64k, 80k, 96k, 112k, 128k, 160k, 192k, 224k, 256k, 320k
                if (q_val != 8. && q_val != 16. && q_val != 24. && q_val != 32. && q_val != 40. && q_val != 48. && q_val != 64. && q_val != 80. &&
                    q_val != 96. && q_val != 112. && q_val != 128. && q_val != 160. && q_val != 192. && q_val != 224. && q_val != 256. && q_val != 320. && verbose) {
                    pwarnf("Given quality '%s' not supported by libmp3lame, encoder will clamp value\n", audio_quality_opt);
                }

                sprintf_s(quality, 24, "-b:a %s", audio_quality_opt);
            } else {
                perrf("Unknown quality '%s' for encoder '%s'", audio_quality_opt, file->args.audio_args.encoder);

                exit(1);
            }
        } else if(isdigit(quality_suffix) != 0) {
            if (strcmp(file->args.audio_args.encoder, AAC_CODEC) == 0) {
                if (0.1 <= q_val && q_val <= 11.9) {
                    sprintf_s(quality, 24, "-q:a %s", audio_quality_opt);
                } else if (q_val < 0.1) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for AAC, clamping to 0.1\n", audio_quality_opt);
                    }

                    strcpy_s(quality, 24, "-q:a 0.1");
                } else if (q_val > 11.9) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for AAC, clamping to 11.9\n", audio_quality_opt);
                    }

                    strcpy_s(quality, 24, "-q:a 11.9");
                }
            } else if (strcmp(file->args.audio_args.encoder, FLAC_CODEC) == 0) {
                if (0. <= q_val && q_val <= 12.5) {
                    sprintf_s(quality, 24, "-compression_level %s", audio_quality_opt);
                } else if (q_val < 0.) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for FLAC, clamping to 0\n", audio_quality_opt);
                    }

                    strcpy_s(quality, 24, "-compression_level 0");
                } else if (q_val > 12.5) {
                    if (verbose) {
                        pwarnf("Given quality '%s' out of range for FLAC, clamping to 12.5\n", audio_quality_opt);
                    }

                    strcpy_s(quality, 24, "-compression_level 12.5");
                }
            } else {
                perrf("Quality '%s' not supported by '%s'", audio_quality_opt, audio_codec_opt);

                exit(1);
            }
        } else if (strcmp(file->args.audio_args.encoder, PCM_F32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_F64_CODEC) == 0 || 
            strcmp(file->args.audio_args.encoder, PCM_S16_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S24_CODEC) == 0 || 
            strcmp(file->args.audio_args.encoder, PCM_S32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S64_CODEC) == 0) {
            if (verbose) {
                pwarnf("Dropping quality '%s' in favour of codec '%s'\n", audio_quality_opt, audio_codec_opt);
            }

            strcpy_s(quality, 24, "");
        } else {
            perrf("Unknown quality '%s'\n", audio_quality_opt);

            exit(1);
        }
    } else {
        if (strcmp(file->args.audio_args.encoder, FLAC_CODEC) == 0) {
            strcpy_s(quality, 24, AUDIO_QUALITY_FALLBACK_FLAC);
        } else if (strcmp(file->args.audio_args.encoder, OPUS_CODEC) == 0) {
            strcpy_s(quality, 24, AUDIO_QUALITY_FALLBACK_OPUS);
        } else if (strcmp(file->args.audio_args.encoder, VORBIS_CODEC) == 0) {
            strcpy_s(quality, 24, AUDIO_QUALITY_FALLBACK_VORBIS);
        } else if (strcmp(file->args.audio_args.encoder, AAC_CODEC) == 0) {
            strcpy_s(quality, 24, AUDIO_QUALITY_FALLBACK_AAC);
        } else if (strcmp(file->args.audio_args.encoder, MP3_CODEC) == 0) {
            strcpy_s(quality, 24, AUDIO_QUALITY_FALLBACK_MP3);
        } else if (strcmp(file->args.audio_args.encoder, PCM_F32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_F64_CODEC) == 0 ||
            strcmp(file->args.audio_args.encoder, PCM_S16_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S24_CODEC) == 0 ||
            strcmp(file->args.audio_args.encoder, PCM_S32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S64_CODEC) == 0) {
            strcpy_s(quality, 24, "");
        }

        if (strlen(quality) > 0) {
            pwarnf("Quality not specifed, using fallback '%s'\n", quality);
        }
    }

    file->args.audio_args.quality = quality;

    char* sample_fmt = malloc(18);
    if (audio_sample_format_opt) {
        if (strcmp(file->args.audio_args.encoder, FLAC_CODEC) == 0) {
            if (strcmp(audio_sample_format_opt, "s16") == 0 || strcmp(audio_sample_format_opt, "s32") == 0) {
                sprintf_s(sample_fmt, 18, "-sample_size %s", audio_sample_format_opt);
            } else {
                perrf("Sample format '%s' not supported by '%s'\n", audio_sample_format_opt, file->args.audio_args.encoder);

                exit(1);
            }
        } else if (strcmp(file->args.audio_args.encoder, OPUS_CODEC) == 0) {
            if (strcmp(audio_sample_format_opt, "flt") == 0) {
                sprintf_s(sample_fmt, 18, "-sample_size %s", audio_sample_format_opt);
            } else {
                perrf("Sample format '%s' is not supported by '%s'\n", audio_sample_format_opt, file->args.audio_args.encoder);

                exit(1);
            }
        } else if (strcmp(file->args.audio_args.encoder, VORBIS_CODEC) == 0 || strcmp(file->args.audio_args.encoder, AAC_CODEC) == 0) {
            if (strcmp(audio_sample_format_opt, "fltp") == 0) {
                sprintf_s(sample_fmt, 18, "-sample_size %s", audio_sample_format_opt);
            } else {
                perrf("Sample format '%s' is not supported by '%s'\n", audio_sample_format_opt, file->args.audio_args.encoder);

                exit(1);
            }
        } else if (strcmp(file->args.audio_args.encoder, MP3_CODEC) == 0){
            if (strcmp(audio_sample_format_opt, "s16p") == 0) {
                sprintf_s(sample_fmt, 18, "-sample_size %s", audio_sample_format_opt);
            } else {
                perrf("Sample format '%s' is not supported by '%s'\n", audio_sample_format_opt, file->args.audio_args.encoder);

                exit(1);
            }
        } else if (strcmp(file->args.audio_args.encoder, PCM_F32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_F64_CODEC) == 0 ||
            strcmp(file->args.audio_args.encoder, PCM_S16_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S24_CODEC) == 0 ||
            strcmp(file->args.audio_args.encoder, PCM_S32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S64_CODEC) == 0) {
            if (verbose) {
                pwarnf("Dropping sample format '%s' in favour of codec '%s'\n", audio_sample_format_opt, audio_codec_opt);
            }

            strcpy_s(sample_fmt, 18, "");
        }
    } else {
        if (strcmp(file->args.audio_args.encoder, FLAC_CODEC) == 0) {
            strcpy_s(sample_fmt, 18, "-sample_fmt s16");
        } else if (strcmp(file->args.audio_args.encoder, MP3_CODEC) == 0) {
            strcpy_s(sample_fmt, 18, "-sample_fmt s16p");
        } else if (strcmp(file->args.audio_args.encoder, PCM_F32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_F64_CODEC) == 0 ||
            strcmp(file->args.audio_args.encoder, PCM_S16_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S24_CODEC) == 0 ||
            strcmp(file->args.audio_args.encoder, PCM_S32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S64_CODEC) == 0) {
            strcpy_s(sample_fmt, 18, "");
        } else if (strcmp(file->args.audio_args.encoder, OPUS_CODEC) == 0) {
            strcpy_s(sample_fmt, 18, "-sample_fmt flt");
        } else if (strcmp(file->args.audio_args.encoder, VORBIS_CODEC) == 0 || strcmp(file->args.audio_args.encoder, AAC_CODEC) == 0) {
            strcpy_s(sample_fmt, 18, "-sample_fmt fltp");
        }

        if (strlen(sample_fmt) > 0 && strcmp(sample_fmt, "-sample_fmt fltp") != 0 && strcmp(sample_fmt, "-sample_fmt flt") != 0 && verbose) {
            printf("Sample format not specified, using fallback '%s'\n", sample_fmt);
        }
    }

    file->args.audio_args.sample_fmt = sample_fmt;

    if (strcmp(file->args.audio_args.encoder, FLAC_CODEC) == 0) {
        strcpy_s(file->output.ext, _MAX_EXT, ".flac");
    } else if (strcmp(file->args.audio_args.encoder, OPUS_CODEC) == 0 || strcmp(file->args.audio_args.encoder, VORBIS_CODEC) == 0) {
        strcpy_s(file->output.ext, _MAX_EXT, ".ogg");
    } else if (strcmp(file->args.audio_args.encoder, AAC_CODEC) == 0) {
        strcpy_s(file->output.ext, _MAX_EXT, ".m4a");
    } else if (strcmp(file->args.audio_args.encoder, MP3_CODEC) == 0) {
        strcpy_s(file->output.ext, _MAX_EXT, ".mp3");
    } else if (strcmp(file->args.audio_args.encoder, PCM_F32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_F64_CODEC) == 0 ||
        strcmp(file->args.audio_args.encoder, PCM_S16_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S24_CODEC) == 0 ||
        strcmp(file->args.audio_args.encoder, PCM_S32_CODEC) == 0 || strcmp(file->args.audio_args.encoder, PCM_S64_CODEC) == 0) {
        strcpy_s(file->output.ext, _MAX_EXT, ".wav");
    }
}