#define _GNU_SOURCE  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUF_SIZE 1024

static void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

static void afficherPlateauClient(int plateau[], int score0, int score1,
                                  const char *j0, const char *j1,
                                  int nextPlayer, const char *me,
                                  int playerIndex, int isObserver) {
    clear_screen();
    printf("=================== PLATEAU ACTUEL ===================\n\n");
    printf("%-15s : ", j1);
    for (int i = 11; i >= 6; i--)
        printf(" %2d ", plateau[i]);
    printf("| Score : %d\n\n", score1);

    printf("%-15s : ", j0);
    for (int i = 0; i <= 5; i++)
        printf(" %2d ", plateau[i]);
    printf("| Score : %d\n\n", score0);

    if (j0[0] && j1[0]) {
        if (isObserver)
            printf("Mode spectateur — prochain tour : %s\n", (nextPlayer == 0 ? j0 : j1));
        else if (playerIndex == nextPlayer)
            printf("C’est votre tour !\n");
        else {
            const char *adv = (strcmp(me, j0) == 0) ? j1 : j0;
            printf("En attente de %s...\n", adv);
        }
    }

    printf("=======================================================\n\n");
    fflush(stdout);
}

static void afficherCommandes(int loggedIn) {
    printf("Commandes disponibles :\n");
    if (!loggedIn) {
        printf("  (Suivez les invites de connexion du serveur)\n\n");
        return;
    }
    printf("  LIST                         → voir les joueurs en ligne\n");
    printf("  GAMES / LIST_GAMES           → voir les parties en cours\n");
    printf("  OBSERVE <id>                 → observer une partie\n");
    printf("  OUT_OBSERVER                 → quitter le mode observateur\n");
    printf("  CHALLENGE <user>             → défier un joueur\n");
    printf("  ACCEPT <user>                → accepter un défi\n");
    printf("  REFUSE <user>                → refuser un défi\n");
    printf("  MOVE <0-11>                  → jouer une case\n");
    printf("  CANCEL_GAME                  → abandonner la partie en cours\n");
    printf("  MESSAGE <user> <message>     → message privé\n");
    printf("  SAY <message>                → discuter globalement\n");
    printf("  BIO <texte 10 lignes max>    → définir/mettre à jour votre bio\n");
    printf("  SHOWBIO <user>               → afficher la bio d’un joueur\n");
    printf("  FRIEND <user>                → ajouter un ami\n");
    printf("  UNFRIEND <user>              → retirer un ami\n");
    printf("  PRIVATE ON|OFF               → activer/désactiver le mode privé\n");
    printf("  QUIT                         → quitter\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("inet_pton"); return EXIT_FAILURE;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); return EXIT_FAILURE;
    }

    printf("Connecté au serveur %s:%s\n\n", argv[1], argv[2]);
    printf("Astuce : suivez les invites. Le serveur commencera par demander votre pseudo.\n\n");

    
    fd_set readfds;
    int maxfd = (sock > STDIN_FILENO ? sock : STDIN_FILENO);
    char buf[BUF_SIZE]; memset(buf, 0, sizeof(buf));

    int loggedIn = 0;
    int expectUsername = 0, expectPassword = 0;
    char pendingUsername[16] = {0};
    char username[16] = {0};

    char joueur0[16] = {0}, joueur1[16] = {0};
    int playerIndex = -1, isObserver = 0;

    int plateau_cache[12] = {0};
    int score0_cache = 0, score1_cache = 0, next_cache = 0;
    int plateau_en_attente = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        
        if (FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) { printf("\nDéconnecté du serveur.\n"); break; }
            buf[n] = '\0';

            char *saveptr = NULL;
            char *line = strtok_r(buf, "\n", &saveptr);
            while (line) {
                line[BUF_SIZE - 1] = '\0';

                
                if (strncmp(line, "ENTER YOUR USERNAME:", 20) == 0) {
                    expectUsername = 1;
                    printf("%s\n", line);
                }
                else if (strncmp(line, "Nice to meet you again,", 23) == 0 ||
                         strncmp(line, "Welcome,", 8) == 0) {
                    expectPassword = 1;
                    printf("%s\n", line);
                }
                else if (strncmp(line, "OK Logged in successfully", 25) == 0 ||
                         strncmp(line, "OK New account created and logged in", 35) == 0 ||
                         strncmp(line, "OK New account created", 23) == 0) {
                    loggedIn = 1;
                    expectUsername = expectPassword = 0;
                    size_t uname_len = strlen(pendingUsername);
                    if (uname_len > 14) uname_len = 14;
                    memcpy(username, pendingUsername, uname_len);
                    username[uname_len] = '\0';
                    printf("%s\n", line);
                    afficherCommandes(loggedIn);
                }
                else if (strncmp(line, "ERR Wrong password", 18) == 0) {
                    loggedIn = 0; expectUsername = 0; expectPassword = 0;
                    pendingUsername[0] = '\0';
                    printf("%s\n", line);
                }

                
                else if (strncmp(line, "GAME_START ", 11) == 0) {
                    if (sscanf(line, "GAME_START %15s vs %15s", joueur0, joueur1) == 2) {
                        joueur0[sizeof(joueur0) - 1] = '\0';
                        joueur1[sizeof(joueur1) - 1] = '\0';
                        printf("\nNouvelle partie : %s vs %s\n", joueur0, joueur1);
                        playerIndex = (strcmp(username, joueur0) == 0) ? 0 : 1;
                        isObserver = 0;
                        send(sock, "READY\n", 6, 0);
                        if (plateau_en_attente) {
                            afficherPlateauClient(plateau_cache, score0_cache, score1_cache,
                                                  joueur0, joueur1, next_cache,
                                                  username, playerIndex, isObserver);
                            plateau_en_attente = 0;
                        }
                    }
                }
                else if (strncmp(line, "BOARD ", 6) == 0) {
                    int p[12], s0, s1, next;
                    if (sscanf(line,
                               "BOARD %d %d %d %d %d %d %d %d %d %d %d %d | Scores: %d-%d | Next: %d",
                               &p[0], &p[1], &p[2], &p[3], &p[4], &p[5],
                               &p[6], &p[7], &p[8], &p[9], &p[10], &p[11],
                               &s0, &s1, &next) == 15) {
                        if (joueur0[0] == '\0' || joueur1[0] == '\0') {
                            memcpy(plateau_cache, p, sizeof(p));
                            score0_cache = s0; score1_cache = s1; next_cache = next;
                            plateau_en_attente = 1;
                        } else {
                            afficherPlateauClient(p, s0, s1, joueur0, joueur1,
                                                  next, username, playerIndex, isObserver);
                        }
                    }
                }
                else if (strncmp(line, "OK Now observing", 16) == 0) {
                    isObserver = 1;
                    int id; char a[16], b[16];
                    if (sscanf(line, "OK Now observing game %d: %15s vs %15s", &id, a, b) == 3) {
                        a[sizeof(a) - 1] = '\0';
                        b[sizeof(b) - 1] = '\0';
                        size_t a_len = strlen(a);
                        if (a_len > 14) a_len = 14;
                        size_t b_len = strlen(b);
                        if (b_len > 14) b_len = 14;
                        memcpy(joueur0, a, a_len);
                        joueur0[a_len] = '\0';
                        memcpy(joueur1, b, b_len);
                        joueur1[b_len] = '\0';
                        printf("Observation de la partie %d : %s vs %s\n", id, joueur0, joueur1);
                    }
                }
                else if (strncmp(line, "OK Left observation mode.", 25) == 0) {
                    isObserver = 0; joueur0[0] = joueur1[0] = '\0';
                    printf("%s\n", line);
                    afficherCommandes(loggedIn);
                }
                else if (strncmp(line, "GAME_END ", 9) == 0 ||
                         strncmp(line, "OK Game canceled", 16) == 0 ||
                         strncmp(line, "INFO ", 5) == 0) {
                    joueur0[0] = joueur1[0] = '\0'; isObserver = 0;
                    printf("%s\n", line);
                    afficherCommandes(loggedIn);
                }
                else {
                    printf("%s\n", line);
                }

                line = strtok_r(NULL, "\n", &saveptr);
            }
            fflush(stdout);
        }

        
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            buf[strcspn(buf, "\n")] = '\0';

            if (!loggedIn && expectUsername) {
                size_t uname_len = strlen(buf);
                if (uname_len > 14) uname_len = 14;
                memcpy(pendingUsername, buf, uname_len);
                pendingUsername[uname_len] = '\0';
                expectUsername = 0;
            }

            if (strcasecmp(buf, "QUIT") == 0) {
                send(sock, "QUIT\n", 5, 0);
                break;
            }

            strcat(buf, "\n");
            send(sock, buf, strlen(buf), 0);
        }
    }

    close(sock);
    return 0;
}