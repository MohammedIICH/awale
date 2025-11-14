/*************************************************************************
                           Awale -- Game Client (Main)
                             -------------------
    début                : 20/10/2025
    auteurs              : Mohammed Iich et Dame Dieng
    e-mails              : mohammed.iich@insa-lyon.fr et dame.dieng@insa-lyon.fr
    description          : Point d'entrée du client Awalé :
                           - connexion au serveur
                           - boucle principale avec select()
                           - gestion du clavier utilisateur
                           - envoi des commandes au serveur
*************************************************************************/

#define _GNU_SOURCE   /* pour strtok_r */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>      /* strcasecmp */
#include <unistd.h>
#include <sys/types.h>    /* ssize_t */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "client.h"

/* =====================================================================
 *  Boucle principale du client
 *  - connexion TCP
 *  - attente multiplexée avec select()
 *  - réception des messages serveur
 *  - lecture et envoi des commandes utilisateur
 * ===================================================================== */
int client_run(const char *server_ip, const char *server_port)
{
    /* -------------------------
     *  Création socket client
     * ------------------------- */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* Préparation adresse serveur */
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(atoi(server_port));

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return EXIT_FAILURE;
    }

    /* Connexion */
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return EXIT_FAILURE;
    }

    /* -------------------------
     *  Initialisation de l'état
     * ------------------------- */
    ClientState state;
    client_state_init(&state, sock);

    ui_print_banner();

    fd_set readfds;
    int maxfd = (sock > STDIN_FILENO ? sock : STDIN_FILENO);

    char recv_buf[CLIENT_BUF_SIZE];
    char input_buf[CLIENT_BUF_SIZE];

    memset(recv_buf, 0, sizeof(recv_buf));
    memset(input_buf, 0, sizeof(input_buf));

    /* =====================================================================
     *  Boucle principale : multiplexage serveur + stdin
     * ===================================================================== */
    int prompt_needed = 1;  /* Track if we need to show the prompt */

    while (1) {

        /* Affichage du prompt si nécessaire */
        if (prompt_needed) {
            ui_print_prompt(&state);
            prompt_needed = 0;
        }

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        /* =====================================================
         *  Réception des messages serveur
         * ===================================================== */
        if (FD_ISSET(sock, &readfds)) {

            int n = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
            if (n < 0) {
                perror(COL_RED "recv" COL_RESET);
                printf(COL_RED "Connection error, closing client.\n" COL_RESET);
                break;
            }
            if (n == 0) {
                printf("\n" COL_RED "Disconnected from server!\n" COL_RESET);
                break;
            }

            /* Assurer la terminaison de la chaîne */
            recv_buf[n] = '\0';

            /* Validation : vérifier que le buffer est bien terminé */
            if (!is_buffer_terminated(recv_buf, n + 1)) {
                printf(COL_RED "ERROR: Buffer not properly terminated\n" COL_RESET);
                continue;
            }

            /* Découpage en lignes (protocole basé sur '\n') */
            char *saveptr = NULL;
            char *line = strtok_r(recv_buf, "\n", &saveptr);

            while (line) {
                if (line[0] != '\0') {
                    protocol_handle_server_line(&state, line);
                }
                line = strtok_r(NULL, "\n", &saveptr);
            }

            /* Après réception de messages serveur, on affiche le prompt */
            prompt_needed = 1;
        }

        /* =====================================================
         *  Entrée utilisateur (stdin)
         * ===================================================== */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {

            if (!fgets(input_buf, sizeof(input_buf), stdin)) {
                /* EOF sur stdin → fermer proprement le client */
                printf("\n" COL_RED "End of standard input, closing client.\n" COL_RESET);
                break;
            }

            /* Assurer la terminaison de la chaîne */
            input_buf[sizeof(input_buf) - 1] = '\0';

            /* Retirer le '\n' final */
            size_t newline_pos = strcspn(input_buf, "\n");
            input_buf[newline_pos] = '\0';

            /* Validation : ignorer les lignes vides */
            if (newline_pos == 0 || input_buf[0] == '\0') {
                prompt_needed = 1;
                continue;
            }

            /* Validation : limiter la longueur (sécurité) */
            if (newline_pos > 200) {
                printf(COL_RED "ERROR: Command too long (max 200 chars)\n" COL_RESET);
                prompt_needed = 1;
                continue;
            }

            /* Commande locale : HELP (n'est pas envoyé au serveur) */
            if (strcasecmp(input_buf, "HELP") == 0) {
                printf("\n\n");
                ui_print_commands(&state);
                printf("\n");
                prompt_needed = 1;
                continue;
            }

            /* Gestion locale du pseudo en phase de login :
             * on mémorise le username tapé pour l'afficher après succès. */
            if (!state.logged_in && state.expect_username) {
                safe_strcpy_client(state.pending_username,
                                   input_buf,
                                   sizeof(state.pending_username));
                state.expect_username = 0;
            }

            /* QUIT : demande explicite de fermeture par l'utilisateur */
            if (strcasecmp(input_buf, "QUIT") == 0) {
                const char *quit_msg = "QUIT\n";
                ssize_t sent = send(sock, quit_msg, strlen(quit_msg), 0);
                if (sent < 0) {
                    perror(COL_RED "send" COL_RESET);
                }
                break;
            }

            /* Envoi de la ligne brute au serveur (commande ou réponse login) */
            size_t len = strlen(input_buf);

            if (len + 1 < sizeof(input_buf)) {
                input_buf[len] = '\n';
                input_buf[len + 1] = '\0';
            } else {
                /* Sécurité : si trop plein, on force juste un '\n' final */
                input_buf[sizeof(input_buf) - 2] = '\n';
                input_buf[sizeof(input_buf) - 1] = '\0';
            }

            ssize_t sent = send(sock, input_buf, strlen(input_buf), 0);
            if (sent < 0) {
                perror(COL_RED "send" COL_RESET);
                break;
            }

            /* After sending command, we need the prompt again */
            prompt_needed = 1;
        }
    }

    close(sock);
    return 0;
}

/* =====================================================================
 *  Point d'entrée du binaire client
 * ===================================================================== */
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <server_ip> <port>\n"
                "Example: %s 127.0.0.1 4444\n",
                argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    return client_run(argv[1], argv[2]);
}