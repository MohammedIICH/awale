/*************************************************************************
                           Awale -- Game Client (Protocol)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    description          : Handles messages received from the server.
                           Updates local state and triggers UI rendering.
*************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "client.h"

/* ============================================================
 *               CLEAN SERVER MESSAGE OUTPUT
 * ============================================================ */
static void print_server_text(const char *msg)
{
    printf("%s\n", msg);
    fflush(stdout);
}

/* ============================================================
 *                  INITIALIZE CLIENT STATE
 * ============================================================ */
void client_state_init(ClientState *state, int sock)
{
    memset(state, 0, sizeof(*state));
    state->sock = sock;
    state->player_index = -1;
}

/* ============================================================
 *             MAIN PROTOCOL MESSAGE PROCESSOR
 * ============================================================ */
void protocol_handle_server_line(ClientState *state, const char *line)
{
    if (!line) return;

    char buf[CLIENT_BUF_SIZE];
    memset(buf, 0, sizeof(buf));
    
    safe_strcpy_client(buf, line, sizeof(buf));
    buf[strcspn(buf, "\r")] = '\0';

    /* ============================================================
     *                     LOGIN : USERNAME
     * ============================================================ */
    if (strncmp(buf, "Enter your username", 19) == 0) {

        ui_clear_screen();
        ui_print_banner();

        printf("\n%s\n", buf);

        state->expect_username = 1;
        state->expect_password = 0;
        state->pending_username[0] = '\0';
        return;
    }

    /* ============================================================
     *                     LOGIN : PASSWORD
     * ============================================================ */
    if (strstr(buf, "Enter your password") ||
        strstr(buf, "Please set your password"))
    {
        state->expect_password = 1;
        print_server_text(buf);
        return;
    }

    /* ============================================================
     *                  LOGIN SUCCESS
     * ============================================================ */
    if (strstr(buf, "Logged in successfully") ||
        strstr(buf, "New account created and logged in"))
    {
        state->logged_in = 1;
        state->expect_username = 0;
        state->expect_password = 0;

        if (!is_string_empty(state->pending_username)) {
            safe_strcpy_client(state->username,
                              state->pending_username,
                              sizeof(state->username));
        }

        ui_clear_screen();
        print_server_text(buf);
        printf("\n");

        ui_print_commands(state);
        return;
    }

    /* ============================================================
     *                 WRONG PASSWORD / RESET LOGIN
     * ============================================================ */
    if (strncmp(buf, "ERROR : Wrong password", 22) == 0) {

        safe_strinit(state->pending_username, sizeof(state->pending_username));
        safe_strinit(state->username, sizeof(state->username));
        state->expect_username     = 0;
        state->expect_password     = 0;

        print_server_text(buf);
        return;
    }

    /* ============================================================
     *                 GAME START
     * ============================================================ */
    if (strncmp(buf, "GAME_START ", 11) == 0) {

        char p0[16];
        char p1[16];
        memset(p0, 0, sizeof(p0));
        memset(p1, 0, sizeof(p1));

        if (sscanf(buf, "GAME_START %15s vs %15s", p0, p1) == 2) {

            safe_strcpy_client(state->player0_name, p0, sizeof(state->player0_name));
            safe_strcpy_client(state->player1_name, p1, sizeof(state->player1_name));

            state->in_game     = 1;
            state->is_observer = 0;
            state->has_players = 1;

            if (safe_strcasecmp(state->username, state->player0_name) == 0)
                state->player_index = 0;
            else if (safe_strcasecmp(state->username, state->player1_name) == 0)
                state->player_index = 1;
            else
                state->player_index = -1;

            ui_clear_screen();
            printf(COL_GREEN "New game started : %s vs %s\n" COL_RESET,
                   state->player0_name, state->player1_name);

            send(state->sock, "READY\n", 6, 0);
        }
        return;
    }

    /* ============================================================
     *                     BOARD UPDATE
     * ============================================================ */
    if (strncmp(buf, "BOARD ", 6) == 0) {

        int p[12];
        int s0, s1, next;
        
        memset(p, 0, sizeof(p));

        if (sscanf(
                buf,
                "BOARD %d %d %d %d %d %d %d %d %d %d %d %d | Scores: %d-%d | Next: %d",
                &p[0], &p[1], &p[2], &p[3], &p[4], &p[5],
                &p[6], &p[7], &p[8], &p[9], &p[10], &p[11],
                &s0, &s1, &next
            ) == 15)
        {
            /* Valider les graines sur le plateau */
            int valid = 1;
            for (int i = 0; i < 12; i++) {
                if (p[i] < 0 || p[i] > 48) {
                    valid = 0;
                    break;
                }
            }
            
            /* Valider les scores */
            if (!is_valid_score(s0) || !is_valid_score(s1)) {
                valid = 0;
            }
            
            /* Joueur suivant */
            if (!is_valid_player_index(next)) {
                valid = 0;
            }
            
            if (valid) {
                memcpy(state->board, p, sizeof(p));
                state->score0 = s0;
                state->score1 = s1;
                state->next_player = next;

                ui_print_board(state);
            }
        }

        return;
    }

    /* ============================================================
     *                  OBSERVATION MODE
     * ============================================================ */
    if (strncmp(buf, "Now observing game", 18) == 0) {

        int id;
        char p0[16];
        char p1[16];
        memset(p0, 0, sizeof(p0));
        memset(p1, 0, sizeof(p1));

        if (sscanf(buf,
                   "Now observing game %d: %15s vs %15s",
                   &id, p0, p1) == 3)
        {
            safe_strcpy_client(state->player0_name, p0, sizeof(state->player0_name));
            safe_strcpy_client(state->player1_name, p1, sizeof(state->player1_name));

            state->is_observer = 1;
            state->in_game     = 0;
            state->has_players = 1;
            state->player_index = -1;

            ui_clear_screen();
            printf("Observing game %d : %s vs %s\n",
                   id, state->player0_name, state->player1_name);

            ui_print_board(state);
        }
        return;
    }

    if (strncmp(buf, "Left observation mode", 21) == 0) {
        state->is_observer = 0;
        state->has_players = 0;
        safe_strinit(state->player0_name, sizeof(state->player0_name));
        safe_strinit(state->player1_name, sizeof(state->player1_name));

        print_server_text(buf);
        printf("\n");
        ui_print_commands(state);
        return;
    }

    /* ============================================================
     *           GAME END / CANCEL / INFO MESSAGES
     * ============================================================ */
    if (strncmp(buf, "GAME_END", 8) == 0 ||
        strncmp(buf, "GAME_CANCELED", 13) == 0 ||
        strstr(buf, "canceled") ||
        strstr(buf, "INFO"))
    {
        state->in_game = 0;
        state->is_observer = 0;
        state->has_players = 0;
        state->player_index = -1;

        safe_strinit(state->player0_name, sizeof(state->player0_name));
        safe_strinit(state->player1_name, sizeof(state->player1_name));

        if (strncmp(buf, "GAME_CANCELED", 13) == 0) {
            char player[32];
            memset(player, 0, sizeof(player));
            if (sscanf(buf, "GAME_CANCELED %31s", player) == 1) {
                printf(COL_YELLOW "⚠ Game cancelled by %s. Back to menu.\n" COL_RESET, player);
            } else {
                print_server_text(buf);
            }
        } else {
            print_server_text(buf);
        }
        printf("\n");
        ui_print_commands(state);
        return;
    }

    /* ============================================================
     *                    EVERYTHING ELSE
     * ============================================================ */
    print_server_text(buf);
}
