#include "read_args.h"

Params process_arguments(const int argc, char * const argv[]) {
    Params parameters = {
        .n = 5,
        .simulation_time = 5.0,
        .box_radius = 10.0 * 1e3,
        .ups = 1000.0,
        .fps = 50.0,
        .resolution = 240,
        .avg_speed = 20.0 * 1e3,
        .output_filename = "animation.gif"
    };

    struct option long_options[] = {
        {"n",           required_argument, NULL, 'n'},
        {"time",        required_argument, NULL, 't'},
        {"box-radius",  required_argument, NULL, 'b'},
        {"avg-speed",   required_argument, NULL, 's'},
        {"ups",         required_argument, NULL, 'u'},
        {"fps",         required_argument, NULL, 'f'},
        {"resolution",  required_argument, NULL, 'r'},
        {"output-file", required_argument, NULL, 'o'},
        {0, 0, 0, 0}
    };

    int ch;
    int opt_index;
    while (-1 != (ch = getopt_long(argc, argv, "t:n:", long_options, &opt_index))) {
        int items_read;

        switch (ch) {
            case 'n':
                items_read = sscanf(optarg, " %d", &parameters.n);
                break;
            
            case 't':
                items_read = sscanf(optarg, " %f", &parameters.simulation_time);
                break;
            
            case 'b':
                items_read = sscanf(optarg, " %f", &parameters.box_radius);
                parameters.box_radius *= 1e3;
                break;
            
            case 's':
                items_read = sscanf(optarg, " %f", &parameters.avg_speed);
                parameters.avg_speed *= 1e3;
                break;
            
            case 'u':
                items_read = sscanf(optarg, " %f", &parameters.ups);
                break;
            
            case 'f':
                items_read = sscanf(optarg, " %f", &parameters.fps);
                break;
            
            case 'r':
                items_read = sscanf(optarg, " %d", &parameters.resolution);
                break;
            
            case 'o':
                parameters.output_filename = optarg;
                items_read = 1;
                break;
            
            case '?':
            default:
                items_read = 0;
                break;
        }

        if (items_read != 1) {
            fprintf(stderr, "Unrecognized argument '%s' for option '%s'.\n", optarg, long_options[opt_index].name);
            exit(EXIT_FAILURE);
        }
    }

    assert(parameters.simulation_time > 0);
    assert(parameters.box_radius > 0);
    assert(parameters.ups > 0);
    assert(parameters.fps > 0);
    assert(parameters.resolution > 0);

    parameters.frame_W = parameters.resolution;
    parameters.frame_H = parameters.resolution;
    parameters.n_frames = ceilf(parameters.simulation_time * parameters.fps);   // TODO: Make sure this is right
    parameters.n_updates = floorf(parameters.simulation_time * parameters.ups); // TODO: Make sure this is right

    return parameters;
}
