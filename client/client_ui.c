/*************************************************************************
                                                     Awale -- Client de jeu (UI)
                                                         -------------------
        début                : 20/10/2025
        auteurs              : Mohammed Iich et Dame Dieng
        description          : Interface terminale propre, moderne et conviviale
                                                     pour le client en ligne Awale.
*************************************************************************/

#include <stdio.h>
#include <string.h>
#include "client.h"

/* ---------------------------------------------------------
 * Efface l'écran du terminal en utilisant les séquences ANSI standard.
 * Repli sûr si le terminal ne supporte pas ANSI.
 * --------------------------------------------------------- */
void ui_clear_screen(void)
{
    /* Essaie d'effacer l'écran avec ANSI */
    if (printf("\033[2J\033[H") < 0) {
        /* Repli : utiliser des nouvelles lignes si ANSI échoue */
        for (int i = 0; i < 50; i++) {
            printf("\n");
        }
    }
    fflush(stdout);
}

/* ---------------------------------------------------------
 * Affiche une bannière d'accueil claire.
 * --------------------------------------------------------- */
void ui_print_banner(void)
{
    ui_clear_screen();

    printf(COL_CYAN "═════════════════════════════════════════════════════════\n" COL_RESET);
    printf(COL_BOLD COL_CYAN "                   Awale Online Client                \n" COL_RESET);
    printf(COL_CYAN "═════════════════════════════════════════════════════════\n\n" COL_RESET);

    printf("You are successfully connected to the server.\n");
    printf("Type " COL_YELLOW COL_BOLD "HELP" COL_RESET " at any time to show the command list.\n");
    printf("Type " COL_RED COL_BOLD "QUIT" COL_RESET " to exit the client.\n\n");
}

/* ---------------------------------------------------------
 * Affiche la liste de commandes contextuelle selon l'état.
 * --------------------------------------------------------- */
void ui_print_commands(const ClientState *state)
{
    /* Valide l'entrée */
    if (!state) {
        printf(COL_RED "ERROR: Invalid state\n" COL_RESET);
        return;
    }

    /* Séparateur */
    printf(COL_CYAN "═══════════════════════════════════════════════════════════════\n" COL_RESET);
    printf(COL_YELLOW "Available commands:\n" COL_RESET);
    printf(COL_CYAN "═══════════════════════════════════════════════════════════════\n" COL_RESET);

    /* ---------- AVANT CONNEXION ---------- */
    if (!state->logged_in)
    {
        printf("  Follow the server prompts to authenticate.\n");
        printf("  Local commands:\n");
        printf("    HELP                       → Show this help\n\n");
        return;
    }

    /* ---------- COMPTE & PROFIL ---------- */
    printf(COL_GREEN "== Account & Profile ==\n" COL_RESET);
    printf("  HELP                         → Show this help menu\n");
    printf("  BIO <text>                   → Set or update your biography\n");
    printf("  SHOWBIO <user>               → Display another player's biography\n");
    printf("  MY_FRIENDS                   → List your friends\n");
    printf("  FRIEND <user>                → Send a friend request\n");
    printf("  ACCEPT_FRIEND <user>         → Accept a friend request\n");
    printf("  DECLINE_FRIEND <user>        → Decline a friend request\n");
    printf("  UNFRIEND <user>              → Remove a player from your friends list\n");
    printf("  PRIVATE ON|OFF               → Enable or disable private mode\n\n");

    /* ---------- COMMANDES DE JEU ---------- */
    printf(COL_GREEN "== Game ==\n" COL_RESET);
    printf("  LIST                         → Show online players\n");
    printf("  GAMES                        → List active games\n");
    printf("  CHALLENGE <user>             → Challenge a player\n");
    printf("  ACCEPT <user>                → Accept a challenge\n");
    printf("  REFUSE <user>                → Refuse a challenge\n");
    printf("  MOVE <0-11>                  → Play a move during a game\n");
    printf("  CANCEL_GAME                  → Abort the current game\n\n");

    /* ---------- MODE OBSERVATEUR ---------- */
    printf(COL_GREEN "== Observer Mode ==\n" COL_RESET);
    printf("  OBSERVE <id>                 → Observe an active game\n");
    printf("  OUT_OBSERVER                 → Leave observer mode\n\n");

    /* ---------- SOCIAL ---------- */
    printf(COL_GREEN "== Social & Chat ==\n" COL_RESET);
    printf("  SAY <message>                → Public chat message\n");
    printf("  MESSAGE <user> <message>     → Private message\n\n");

    /* ---------- DIVERS ---------- */
    printf(COL_GREEN "== Misc ==\n" COL_RESET);
    printf("  QUIT                         → Exit the client\n\n");

    /* ---------- INFORMATIONS CONTEXTUELLES ---------- */
    if (state->in_game) {
        printf(COL_CYAN "You are currently in a game.\n" COL_RESET);
        printf("  → Key commands: MOVE, CANCEL_GAME, SAY, MESSAGE\n\n");
    }
    else if (state->is_observer) {
        printf(COL_CYAN "You are observing a game.\n" COL_RESET);
        printf("  → Key commands: OUT_OBSERVER, SAY, MESSAGE\n\n");
    }
    else {
        printf("You are connected.\n");
        printf("  → Key commands: LIST, GAMES, CHALLENGE, BIO, FRIEND\n\n");
    }

    printf(COL_CYAN "═══════════════════════════════════════════════════════════════\n" COL_RESET);
}

/* ---------------------------------------------------------
 * Affiche le plateau Awale avec les noms des joueurs et les scores.
 * --------------------------------------------------------- */
void ui_print_board(const ClientState *state)
{
    /* Valide l'entrée */
    if (!state) {
        printf(COL_RED "ERROR: Invalid state\n" COL_RESET);
        return;
    }

    ui_clear_screen();

    if (!state->has_players) {
        printf(COL_YELLOW "Waiting for game information...\n" COL_RESET);
        return;
    }

    printf(COL_CYAN "====================== CURRENT BOARD ======================\n\n" COL_RESET);

    /* Joueur supérieur (player1) */
    printf("%-12s : ", state->player1_name[0] ? state->player1_name : "Player 1");
    for (int i = 11; i >= 6; i--) {
        if (!is_valid_board_index(i)) {
            printf(COL_RED "[ERR]" COL_RESET);
            continue;
        }
        printf(" %2d ", state->board[i]);
    }
    printf(" | Score : ");
    if (!is_valid_score(state->score1)) {
        printf(COL_RED "[ERR]" COL_RESET);
    } else {
        printf("%d", state->score1);
    }
    printf("\n\n");

    /* Joueur inférieur (player0) */
    printf("%-12s : ", state->player0_name[0] ? state->player0_name : "Player 0");
    for (int i = 0; i <= 5; i++) {
        if (!is_valid_board_index(i)) {
            printf(COL_RED "[ERR]" COL_RESET);
            continue;
        }
        printf(" %2d ", state->board[i]);
    }
    printf(" | Score : ");
    if (!is_valid_score(state->score0)) {
        printf(COL_RED "[ERR]" COL_RESET);
    } else {
        printf("%d", state->score0);
    }
    printf("\n\n");

    /* Informations contextuelles sur le tour */
    if (state->is_observer) {
        const char *next =
            (state->next_player == 0) ? state->player0_name : state->player1_name;
        printf("Observer mode — next move: %s\n", next);
    }
    else if (state->in_game) {
        if (state->player_index == state->next_player)
            printf(COL_GREEN "Your turn!\n" COL_RESET);
        else {
            const char *adv = NULL;
            if (safe_strcasecmp(state->username, state->player0_name) == 0)
                adv = state->player1_name;
            else if (safe_strcasecmp(state->username, state->player1_name) == 0)
                adv = state->player0_name;
            
            if (adv && !is_string_empty(adv))
                printf("Waiting for %s...\n", adv);
            else
                printf("Waiting for opponent...\n");
        }
    }

    printf(COL_CYAN "\n===========================================================\n\n" COL_RESET);
}

/* ---------------------------------------------------------
 * Affiche l'invite de commande contextuelle selon l'état.
 * --------------------------------------------------------- */
void ui_print_prompt(const ClientState *state)
{
    if (!state) {
        printf(COL_RED "[error] > " COL_RESET);
        fflush(stdout);
        return;
    }

    if (!state->logged_in) {
        printf(COL_BLUE "[login] > " COL_RESET);
    }
    else if (state->is_observer) {
        printf(COL_CYAN "[observe] > " COL_RESET);
    }
    else if (state->in_game) {
        printf(COL_GREEN "[game] > " COL_RESET);
    }
    else {
        printf(COL_BLUE "[menu] > " COL_RESET);
    }

    fflush(stdout);
}
