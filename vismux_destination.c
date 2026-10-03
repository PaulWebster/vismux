/***************************************************************
 * FILENAME: vismux.c
 * DESCRIPTION: A low latency, real-time POSIX Shared Memory (SHM) replicator tailored specifically for Squeezelite audio visualizer data.
 * SUMMARY: Pushes visualizer data over a UDP network pipeline, allowing Jivelite and third-party tools like CAVA or projectM to run seamlessly on a completely remote machine on the same LAN.
 * AUTHOR: Paul Webster
 * DATE: 20/Sep/2026
 * MODIFICATION:
 * CHANGES: N/A
 * Copyright (C) 2026 Paul Webster
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Paul Webster - paul@dabdig.com
 ****************************************************************/

#include "vismux.h"

#define SLOT_COUNT  16
static destination_spec_t specs[SLOT_COUNT];
static pthread_t* threads[SLOT_COUNT];

int main(int argc, char *argv[])
{
    int ix_dest = 0;
    bool daemonise = false;
    const char* logfile = NULL;
    pthread_t* ui_thread = NULL;
    bool discoverable = true;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);

#define ARG_AVAIL(n)  if ((i +n) >= argc) { fprintf(stderr, "invalid commandline"); exit(EXIT_FAILURE); }
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--source") == 0 ) {
            ARG_AVAIL(1);
            if (ix_dest < (int)(sizeof(specs)/sizeof(specs[0]))) {
                char* src_ip = strdup(argv[++i]);
                specs[ix_dest].server_ip = src_ip; 
                specs[ix_dest].port = -1;
                specs[ix_dest].mac = NULL;
                specs[ix_dest].keep_running = true;

                char *macp = strchr(src_ip, ',');
                if (macp) {
                    *macp = '\0';
                    ++macp;
                    specs[ix_dest].mac = strdup(macp);
                }
                char *portp = strchr(src_ip, ':');
                if (portp) {
                    *portp = '\0';
                    ++portp;
                    specs[ix_dest].port = atoi(portp);
                }
                struct in_addr server_addr;
                if (inet_pton(AF_INET, src_ip, &server_addr) != 1) {
                    log_msg(-1, "invalid IP address %s", src_ip);
                    exit(EXIT_FAILURE);
                }
                ++ix_dest;
            } else {
                log_msg(-1, "too many sources, max sources = %d", (int)(sizeof(specs)/sizeof(specs[0])));
            }
        }
        else if (strcmp(argv[i], "--port") == 0 ) {
            ARG_AVAIL(1);
            global_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timeout") == 0 ) {
            ARG_AVAIL(1);
            timeout_secs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--mac-timeout") == 0 ) {
            ARG_AVAIL(1);
            mac_timeout_secs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-level") == 0 ) {
            ARG_AVAIL(1);
            log_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--proto-version") == 0 ) {
            ARG_AVAIL(1);
            forced_proto_version = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--remove-shm") == 0) {
            keep_shm = false;
        } else if (strcmp(argv[i], "--stats-int") == 0 ) {
            ARG_AVAIL(1);
            stats_int = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--not-discoverable") == 0) {
            discoverable = false;
        } else if (strcmp(argv[i], "-z") == 0 || strcmp(argv[i], "--daemonise") == 0) {
            daemonise = true;
        } else if ( (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--logfile") == 0)
                && i + 1 < argc) {
            ++i;
            logfile = argv[i];
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("vismux version %s\n", APP_VERSION);
            return 0; // Clean exit immediately
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Squeezelite Replicator Destination v%s\nUsage Options:\n", APP_VERSION);
            printf("%s  <--source <source_ip>[:port][,mac_address]> ", argv[0]);
            printf("  [--mac-timeout <sec>] [--port <p>] [--proto-version <1|2>]  [--not-discoverable] [--remove-shm] [--stats-int <interval_secs>]\n\n");
            printf(" --source can be repeated multiple times, once for each source\n");
            printf("Global Flags:\n");
            printf("  -h, --help        Display this help message\n");
            printf("  -v, --version     Display application version details\n");
            printf("  --log-level <0-3> Filter verbosity (0=ERR, 1=WARN, 2=INFO, 3=DBG)\n\n");
            printf("Interactive Controls (does not require Enter):\n");
            printf("  Press 'v'         Version - Display application version details\n");
            printf("  Press 'q'         Quit - Request shutdown\n");
            printf("  Press 'l'         Log Level - Cycle log levels dynamically (0=ERROR -> 1=WARN -> 2=INFO -> 3=DEBUG)\n");
            printf("  Press 's'         Stats - Request stats summary on next data reception\n");
            return 1;
        } else {
            fprintf(stderr, "Unrecognized option: %s. %s\n", argv[i], HELP_HINT);
            return 1;
        }
    }

    if (NULL != logfile) {
        if (daemonise && !freopen(logfile, "a", stdout)) {
            fprintf(stderr, "Error opening log file %s: %s\n", logfile, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (!freopen(logfile, "a", stderr)) {
            fprintf(stderr, "Error opening log file %s: %s\n", logfile, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (daemonise) {
        if (daemon(0, logfile ? 1: 0)) {
            fprintf(stderr, "Failed to run as daemon: %s\n", strerror(errno));
        }
    } else {
        if (isatty(STDIN_FILENO)) {
            ui_thread = create_thread(NULL, console_listener_thread, NULL);
        }
    }

    for (int i = 0; i < argc; ++i) {
        if (i) {
            printf(" ");
        }
        printf("%s", argv[i]);
    }
    puts("");
    fflush(stdout);

    if (ix_dest == 0) {
        log_msg(-1, "No sources specified, using discovery to add sources.");
        discover_records_t* discovery  = run_discovery_prober(DISCOVER_ROLE_SOURCE);
        for (int ix = 0; ix < discovery->count; ++ix) {
            destination_spec_t* spec = specs + ix_dest;
            peer_record_t* peer = discovery->records +ix;
            char peer_ipaddr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer->ip, peer_ipaddr_str, sizeof(peer_ipaddr_str));
            switch(is_ipaddr_local(peer->ip)){
                case -1:
                    log_msg(-1, "Unable to retrieve local IP addresses");
                    exit(EXIT_FAILURE);
                    break;
                case 0:
                    log_msg(-1, "Ignoring local source : %s:%d MAC:%s", peer_ipaddr_str, (int)peer->port, peer->mac);
                    break;
                case 1:
                    spec->keep_running = true;
                    spec->port = peer->port;
                    spec->mac = strdup(peer->mac);
                    spec->server_ip = strdup(peer_ipaddr_str);
                    inet_ntop(AF_INET, &peer->ip, (char *)spec->server_ip, INET_ADDRSTRLEN);
                    log_msg(-1, "Adding remote source  : %s:%d MAC:%s", spec->server_ip, (int)spec->port, spec->mac);
                    ++ix_dest;
                    break;
            }
        }
    }

    for(int ix =0; ix < (int)(sizeof(specs)/sizeof(specs[0])); ++ix) {
        destination_spec_t* spec = specs + ix;
        spec->discoverable = discoverable;
        if (spec->port < 0) {
            spec->port = global_port;
        }
        if (spec->keep_running && spec->server_ip) {
            threads[ix] = create_thread(NULL, run_destination_thread, spec);
        }
    }

    for(int ix =0; ix < (int)(sizeof(threads)/sizeof(threads[0])); ++ix) {
        join_thread(threads + ix);
    }
    log_msg(2, "Terminating no destination threads are running");
    keep_running = 0;
    join_thread(&ui_thread);

    return 0;
}
