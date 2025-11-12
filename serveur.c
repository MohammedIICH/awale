
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/stat.h>
#include <strings.h> 
#include "jeu.h"

#define MAX_CLIENTS   20
#define MAX_GAMES     (MAX_CLIENTS/2)
#define MAX_ACCOUNTS  256
#define BUF_SIZE      512
#define PORT          4444
#define USERS_FILE    "data/users.txt"

typedef struct {
    char username[16];
    char password[32];
    char bio[512];     
    char friends[256]; 
} Account;

typedef struct {
    int fd;
    char name[16];
    int loggedIn;
    int inGame;
    int opponent;      
    int playerIndex;   
    int ready;
    int loginStage;    
    int privateMode;   
} Client;

typedef struct {
    int active;
    int plateau[12];
    Joueur j0, j1;
    int toMove;                 
    char filename[160];         
    int playerFd0, playerFd1;   
    int observers[MAX_CLIENTS]; 
    int observerCount;
} Game;

Client  clients[MAX_CLIENTS];
Game    games[MAX_GAMES];
Account accounts[MAX_ACCOUNTS];
int     accountCount = 0;

fd_set master_set;
int max_fd;

static void bio_encode(const char *in, char *out, size_t outsz) {
    
    size_t k = 0;
    for (size_t i = 0; in[i] && k+1 < outsz; i++) {
        char c = in[i];
        if (c == '\n') c = '|';
        if (c == ':') c = ' ';
        out[k++] = c;
    }
    out[k] = '\0';
}

static void bio_decode(const char *in, char *out, size_t outsz) {
    size_t k = 0;
    for (size_t i = 0; in[i] && k+1 < outsz; i++) {
        char c = in[i] == '|' ? '\n' : in[i];
        out[k++] = c;
    }
    out[k] = '\0';
}

static int findAccount(const char *username) {
    for (int i = 0; i < accountCount; i++)
        if (strcmp(accounts[i].username, username) == 0) return i;
    return -1;
}

static int isFriend(Account *acc, const char *username) {
    if (!acc || !username || !*username) return 0;
    if (acc->friends[0] == '\0') return 0;
    char tmp[256]; strncpy(tmp, acc->friends, sizeof(tmp)-1); tmp[255]='\0';
    char *tok = strtok(tmp, ",");
    while (tok) {
        if (strcmp(tok, username) == 0) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

static int loadAccounts(void) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[1024];
    int n = 0;
    while (n < MAX_ACCOUNTS && fgets(line, sizeof(line), f)) {
        
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == 0) continue;

        
        char *p1 = strchr(line, ':'); if (!p1) continue; *p1++ = 0;
        char *p2 = strchr(p1,   ':'); if (!p2) continue; *p2++ = 0;
        char *p3 = strchr(p2,   ':'); 
        char *u = line, *pw = p1, *bio_enc = NULL, *friends = NULL;

        if (p3) { *p3++ = 0; bio_enc = p2; friends = p3; }
        else    { bio_enc = p2; friends = ""; }

        
        size_t u_len = strlen(u);
        if (u_len > 15) u_len = 15;
        memcpy(accounts[n].username, u, u_len);
        accounts[n].username[u_len] = '\0';
        
        size_t pw_len = strlen(pw);
        if (pw_len > 31) pw_len = 31;
        memcpy(accounts[n].password, pw, pw_len);
        accounts[n].password[pw_len] = '\0';
        bio_decode(bio_enc, accounts[n].bio, sizeof(accounts[n].bio));
        strncpy(accounts[n].friends, friends ? friends : "", sizeof(accounts[n].friends) - 1);
        accounts[n].friends[sizeof(accounts[n].friends) - 1] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

static void saveAccounts(void) {
    FILE *f = fopen(USERS_FILE, "w");
    if (!f) return;
    for (int i = 0; i < accountCount; i++) {
        char bio_oneline[600];
        bio_encode(accounts[i].bio, bio_oneline, sizeof(bio_oneline));
        fprintf(f, "%s:%s:%s:%s\n",
                accounts[i].username,
                accounts[i].password,
                bio_oneline,
                accounts[i].friends);
    }
    fclose(f);
}

static void broadcast(const char *msg, int except_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1 && clients[i].loggedIn && clients[i].fd != except_fd)
            send(clients[i].fd, msg, strlen(msg), 0);
}
static int clientIndexByName(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1 && clients[i].loggedIn && strcmp(clients[i].name, name)==0)
            return i;
    return -1;
}

static void sendBoard(Game *g) {
    char msg[256];
    snprintf(msg, sizeof(msg),
             "BOARD %d %d %d %d %d %d %d %d %d %d %d %d | Scores: %d-%d | Next: %d\n",
             g->plateau[0], g->plateau[1], g->plateau[2], g->plateau[3],
             g->plateau[4], g->plateau[5], g->plateau[6], g->plateau[7],
             g->plateau[8], g->plateau[9], g->plateau[10], g->plateau[11],
             g->j0.score, g->j1.score, g->toMove);
    if (g->playerFd0 > 0) send(g->playerFd0, msg, strlen(msg), 0);
    if (g->playerFd1 > 0) send(g->playerFd1, msg, strlen(msg), 0);
    for (int z = 0; z < g->observerCount; z++)
        if (g->observers[z] != -1) send(g->observers[z], msg, strlen(msg), 0);
}

static void startGame(int a, int b) {
    int gidx = -1;
    for (int i = 0; i < MAX_GAMES; i++) if (!games[i].active) { gidx = i; break; }
    if (gidx == -1) return;
    Game *g = &games[gidx];
    g->active = 1;
    g->observerCount = 0;
    initialiserJeu(g->plateau);
    reinitialiserScores(&g->j0, &g->j1);
    strncpy(g->j0.pseudo, clients[a].name, 15); g->j0.pseudo[15]='\0';
    strncpy(g->j1.pseudo, clients[b].name, 15); g->j1.pseudo[15]='\0';
    g->toMove = rand() % 2;
    g->playerFd0 = clients[a].fd;
    g->playerFd1 = clients[b].fd;

    clients[a].inGame = clients[b].inGame = 1;
    clients[a].opponent = b; clients[b].opponent = a;
    clients[a].playerIndex = 0; clients[b].playerIndex = 1;
    clients[a].ready = clients[b].ready = 0;

    time_t t = time(NULL);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", localtime(&t));
    snprintf(g->filename, sizeof(g->filename),
             "games/game_%s_vs_%s_%s.txt", clients[a].name, clients[b].name, ts);
    FILE *f = fopen(g->filename, "w");
    if (f) { fprintf(f, "GAME_START %s vs %s\n", clients[a].name, clients[b].name); fclose(f); }

    char msg[128];
    snprintf(msg, sizeof(msg), "GAME_START %s vs %s\n", clients[a].name, clients[b].name);
    send(clients[a].fd, msg, strlen(msg), 0);
    send(clients[b].fd, msg, strlen(msg), 0);
}

static void endGame(Game *g) {
    char endmsg[128];
    snprintf(endmsg, sizeof(endmsg), "GAME_END %d %d\n", g->j0.score, g->j1.score);
    if (g->playerFd0 > 0) send(g->playerFd0, endmsg, strlen(endmsg), 0);
    if (g->playerFd1 > 0) send(g->playerFd1, endmsg, strlen(endmsg), 0);
    for (int z = 0; z < g->observerCount; z++)
        if (g->observers[z] != -1) send(g->observers[z], endmsg, strlen(endmsg), 0);

    FILE *f = fopen(g->filename, "a");
    if (f) {
        fprintf(f, "GAME_END %s: %d %s: %d\n",
                g->j0.pseudo, g->j0.score, g->j1.pseudo, g->j1.score);
        fclose(f);
    }
    g->active = 0;
}

static void processMove(int ci, int pit) {
    if (!clients[ci].inGame) { send(clients[ci].fd, "ERR Not in game\n", 16, 0); return; }

    Game *g = NULL;
    for (int k = 0; k < MAX_GAMES; k++) {
        if (games[k].active &&
            (strcmp(games[k].j0.pseudo, clients[ci].name) == 0 ||
             strcmp(games[k].j1.pseudo, clients[ci].name) == 0)) {
            g = &games[k]; break;
        }
    }
    if (!g) { send(clients[ci].fd, "ERR No game found\n", 18, 0); return; }

    if (clients[ci].playerIndex != g->toMove) {
        send(clients[ci].fd, "ERR Not your turn\n", 18, 0); return;
    }

    int rc = jouerCoup(g->plateau, clients[ci].playerIndex, &g->j0, &g->j1, pit);
    if (rc != 0) { send(clients[ci].fd, "ILLEGAL move\n", 13, 0); return; }

    FILE *f = fopen(g->filename, "a");
    if (f) { fprintf(f, "%s MOVE %d\n", clients[ci].name, pit); fclose(f); }

    g->toMove ^= 1;
    sendBoard(g);

    if (terminerPartie(g->plateau, &g->j0.score, &g->j1.score)) {
        endGame(g);
    }
}

static int canObserve(Game *g, const char *observer) {
    int c0 = clientIndexByName(g->j0.pseudo);
    int c1 = clientIndexByName(g->j1.pseudo);
    int priv0 = (c0 >= 0) ? clients[c0].privateMode : 0;
    int priv1 = (c1 >= 0) ? clients[c1].privateMode : 0;
    if (!priv0 && !priv1) return 1;

    int a0 = findAccount(g->j0.pseudo);
    int a1 = findAccount(g->j1.pseudo);

    if (priv0 && (a0 < 0 || !isFriend(&accounts[a0], observer))) return 0;
    if (priv1 && (a1 < 0 || !isFriend(&accounts[a1], observer))) return 0;
    return 1;
}

static void removeClient(int fd) {
    
    for (int g = 0; g < MAX_GAMES; g++) if (games[g].active) {
        for (int z = 0; z < games[g].observerCount; z++) {
            if (games[g].observers[z] == fd) {
                games[g].observers[z] = games[g].observers[--games[g].observerCount];
                break;
            }
        }
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd == fd) {
        
        if (clients[i].inGame) {
            int opp = clients[i].opponent;
            for (int g = 0; g < MAX_GAMES; g++) if (games[g].active) {
                if (strcmp(games[g].j0.pseudo, clients[i].name)==0 ||
                    strcmp(games[g].j1.pseudo, clients[i].name)==0) {
                    
                    if (opp >= 0 && clients[opp].fd != -1) {
                        send(clients[opp].fd, "INFO Opponent disconnected. Game ended.\n", 41, 0);
                        clients[opp].inGame = 0; clients[opp].ready = 0; clients[opp].opponent = -1;
                    }
                    games[g].active = 0;
                    break;
                }
            }
        }
        close(fd);
        FD_CLR(fd, &master_set);
        clients[i].fd = -1;
        clients[i].loggedIn = 0;
        clients[i].loginStage = 0;
        clients[i].inGame = 0;
        clients[i].ready = 0;
        clients[i].privateMode = 0;
        clients[i].opponent = -1;
        clients[i].name[0] = '\0';
        return;
    }
}

static void handleNewConnection(int server_fd) {
    struct sockaddr_in cli; socklen_t alen = sizeof(cli);
    int newfd = accept(server_fd, (struct sockaddr*)&cli, &alen);
    if (newfd < 0) return;

    for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd == -1) {
        clients[i].fd = newfd;
        clients[i].loggedIn = 0;
        clients[i].loginStage = 0;
        clients[i].inGame = 0;
        clients[i].ready = 0;
        clients[i].opponent = -1;
        clients[i].privateMode = 0;
        clients[i].name[0] = '\0';
        FD_SET(newfd, &master_set);
        if (newfd > max_fd) max_fd = newfd;
        send(newfd, "ENTER YOUR USERNAME:\n", 22, 0);
        return;
    }
    send(newfd, "Server full\n", 12, 0);
    close(newfd);
}

static void handleClientMessage(int fd) {
    char buf[BUF_SIZE];
    int n = recv(fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) { removeClient(fd); return; }
    buf[n] = 0;

    int i;
    for (i = 0; i < MAX_CLIENTS; i++) if (clients[i].fd == fd) break;
    if (i == MAX_CLIENTS) return;

    buf[strcspn(buf, "\r\n")] = 0;

    
    if (!clients[i].loggedIn) {
        if (clients[i].loginStage == 0) {
            if (strlen(buf) == 0) { send(fd, "ENTER YOUR USERNAME:\n", 22, 0); return; }
            
            size_t uname_len = strlen(buf);
            if (uname_len > 14) uname_len = 14;
            memcpy(clients[i].name, buf, uname_len);
            clients[i].name[uname_len] = '\0';
            int idx = findAccount(clients[i].name);
            if (idx >= 0) {
                char msg[80];
                snprintf(msg, sizeof(msg), "Nice to meet you again, %s! Enter your password:\n", clients[i].name);
                send(fd, msg, strlen(msg), 0);
            } else {
                char msg[80];
                snprintf(msg, sizeof(msg), "Welcome, %s! Please set your password:\n", clients[i].name);
                send(fd, msg, strlen(msg), 0);
            }
            clients[i].loginStage = 1;
            return;
        } else { 
            int idx = findAccount(clients[i].name);
            if (idx >= 0) {
                if (strcmp(accounts[idx].password, buf) == 0) {
                    clients[i].loggedIn = 1;
                    clients[i].loginStage = 2;
                    send(fd, "OK Logged in successfully\n", 26, 0);
                } else {
                    send(fd, "ERR Wrong password\nENTER YOUR USERNAME:\n", 40, 0);
                    clients[i].loginStage = 0;
                }
            } else {
                if (accountCount < MAX_ACCOUNTS) {
                    memcpy(accounts[accountCount].username, clients[i].name, 15);
                    accounts[accountCount].username[15] = '\0';
                    
                    size_t pwd_len = strlen(buf);
                    if (pwd_len > 30) pwd_len = 30;
                    memcpy(accounts[accountCount].password, buf, pwd_len);
                    accounts[accountCount].password[pwd_len] = '\0';
                    accounts[accountCount].bio[0] = '\0';
                    accounts[accountCount].friends[0] = '\0';
                    accountCount++; saveAccounts();
                    clients[i].loggedIn = 1; clients[i].loginStage = 2;
                    send(fd, "OK New account created and logged in\n", 36, 0);
                } else {
                    send(fd, "ERR Account storage full\n", 25, 0);
                    clients[i].loginStage = 0;
                }
            }
            return;
        }
    }
    if (strncmp(buf, "LIST", 4) == 0) {
        char list[512] = "ONLINE:";
        for (int j = 0; j < MAX_CLIENTS; j++)
            if (clients[j].fd != -1 && clients[j].loggedIn && j != i && !clients[j].inGame) {
                strcat(list, " "); strcat(list, clients[j].name);
            }
        if (strcmp(list, "ONLINE:") == 0) strcat(list, " (no other players online)");
        strcat(list, "\n"); send(fd, list, strlen(list), 0);
    }
    else if (strncmp(buf, "LIST_GAMES", 10) == 0 || strncmp(buf, "GAMES", 5) == 0) {
        char msg[512] = "ONGOING GAMES:\n"; int c = 0;
        for (int g = 0; g < MAX_GAMES; g++) if (games[g].active) {
            char line[128];
            snprintf(line, sizeof(line), "  ID %d: %s vs %s\n", g, games[g].j0.pseudo, games[g].j1.pseudo);
            strcat(msg, line); c++;
        }
        if (!c) strcat(msg, "  (no active games)\n");
        send(fd, msg, strlen(msg), 0);
    }
    else if (strncmp(buf, "OBSERVE ", 8) == 0) {
        int id; if (sscanf(buf+8, "%d", &id) != 1) { send(fd, "ERR Usage: OBSERVE <id>\n", 24, 0); return; }
        if (id < 0 || id >= MAX_GAMES || !games[id].active) { send(fd, "ERR Invalid game ID\n", 20, 0); return; }
        Game *g = &games[id];
        if (!canObserve(g, clients[i].name)) {
            send(fd, "ERR Game is private. You are not allowed to observe.\n", 53, 0);
            return;
        }
        if (g->observerCount < MAX_CLIENTS) {
            g->observers[g->observerCount++] = fd;
            char ok[128];
            snprintf(ok, sizeof(ok), "OK Now observing game %d: %s vs %s\n", id, g->j0.pseudo, g->j1.pseudo);
            send(fd, ok, strlen(ok), 0);
            sendBoard(g);
        } else {
            send(fd, "ERR Too many observers\n", 23, 0);
        }
    }
    else if (strncmp(buf, "OUT_OBSERVER", 12) == 0) {
        for (int g = 0; g < MAX_GAMES; g++) {
            for (int z = 0; z < games[g].observerCount; z++) {
                if (games[g].observers[z] == fd) {
                    games[g].observers[z] = games[g].observers[--games[g].observerCount];
                    send(fd, "OK Left observation mode. Back to menu.\n", 40, 0);
                    return;
                }
            }
        }
        send(fd, "ERR You are not observing any game\n", 35, 0);
    }
    else if (strncmp(buf, "PRIVATE ", 8) == 0) {
        char mode[8] = {0}; sscanf(buf+8, "%7s", mode);
        if (strcasecmp(mode, "ON") == 0) {
            clients[i].privateMode = 1;
            send(fd, "Private mode ON: only your friends may observe your games.\n", 60, 0);
        } else {
            clients[i].privateMode = 0;
            send(fd, "Private mode OFF: everyone may observe your games.\n", 52, 0);
        }
    }
    else if (strncmp(buf, "FRIEND ", 7) == 0) {
        char target[16]; if (sscanf(buf+7, "%15s", target) != 1) { send(fd, "ERR Usage: FRIEND <user>\n", 25, 0); return; }
        int me = findAccount(clients[i].name), you = findAccount(target);
        if (me < 0 || you < 0) { send(fd, "ERR Unknown user\n", 17, 0); return; }
        if (isFriend(&accounts[me], target)) { send(fd, "Already friends\n", 16, 0); return; }
        
        size_t current_len = strlen(accounts[me].friends);
        size_t target_len = strlen(target);
        if (current_len + target_len + 2 >= sizeof(accounts[me].friends)) {
            send(fd, "ERR Friends list full\n", 22, 0);
            return;
        }
        if (current_len > 0) strncat(accounts[me].friends, ",", sizeof(accounts[me].friends) - current_len - 1);
        strncat(accounts[me].friends, target, sizeof(accounts[me].friends) - strlen(accounts[me].friends) - 1);
        saveAccounts();
        send(fd, "OK Friend added\n", 16, 0);
    }
    else if (strncmp(buf, "UNFRIEND ", 9) == 0) {
        char target[16]; if (sscanf(buf+9, "%15s", target) != 1) { send(fd, "ERR Usage: UNFRIEND <user>\n", 27, 0); return; }
        int me = findAccount(clients[i].name);
        if (me < 0) { send(fd, "ERR Account not found\n", 22, 0); return; }
        char tmp[256];
        tmp[0] = '\0';
        char list[256];
        strncpy(list, accounts[me].friends, sizeof(list) - 1);
        list[sizeof(list) - 1] = '\0';
        char *save_tok = NULL;
        char *tok = strtok_r(list, ",", &save_tok);
        int first = 1, removed = 0;
        while (tok) {
            if (strcmp(tok, target) != 0) {
                size_t tmp_len = strlen(tmp);
                if (!first && tmp_len + 1 < sizeof(tmp)) {
                    strncat(tmp, ",", sizeof(tmp) - tmp_len - 1);
                    tmp_len++;
                }
                if (tmp_len + strlen(tok) < sizeof(tmp)) {
                    strncat(tmp, tok, sizeof(tmp) - tmp_len - 1);
                }
                first = 0;
            } else removed = 1;
            tok = strtok_r(NULL, ",", &save_tok);
        }
        
        size_t tmp_len = strlen(tmp);
        if (tmp_len > 255) tmp_len = 255;
        memcpy(accounts[me].friends, tmp, tmp_len);
        accounts[me].friends[tmp_len] = '\0';
        saveAccounts();
        send(fd, removed ? "OK Friend removed\n" : "No such friend\n", removed ? 18 : 16, 0);
    }
    else if (strncmp(buf, "BIO ", 4) == 0) {
        int idx = findAccount(clients[i].name);
        if (idx >= 0) {
            
            int lines = 0; char cleaned[512]; size_t k = 0;
            size_t remaining = strlen(buf) - 4;
            if (remaining > 510) remaining = 510;  
            for (size_t p = 4; p < 4 + remaining && buf[p] && k+1 < sizeof(cleaned); p++) {
                char c = buf[p];
                if (c == '\n' || c == '\r') continue;
                if (c == '\t') c = ' ';
                if (c == '|') c = '/';
                if (c == '\0') break;
                if (c == '\\' && buf[p+1]=='n') { c = '\n'; p++; }
                if (c == '\n') { if (++lines >= 10) { c=' '; } }
                cleaned[k++] = c;
            }
            cleaned[k] = '\0';
            
            size_t bio_len = strlen(cleaned);
            if (bio_len > 510) bio_len = 510;
            memcpy(accounts[idx].bio, cleaned, bio_len);
            accounts[idx].bio[bio_len] = '\0';
            saveAccounts();
            send(fd, "OK Bio updated\n", 15, 0);
        } else send(fd, "ERR Account not found\n", 22, 0);
    }
    else if (strncmp(buf, "SHOWBIO ", 8) == 0) {
        char target[16]; if (sscanf(buf+8, "%15s", target) != 1) { send(fd, "ERR Usage: SHOWBIO <user>\n", 26, 0); return; }
        int idx = findAccount(target);
        if (idx >= 0) {
            char msg[700];
            snprintf(msg, sizeof(msg), "\n--- BIO of %s ---\n%s\n-----------------\n",
                     accounts[idx].username,
                     accounts[idx].bio[0] ? accounts[idx].bio : "(no bio)");
            send(fd, msg, strlen(msg), 0);
        } else send(fd, "ERR User not found\n", 19, 0);
    }
    else if (strncmp(buf, "CHALLENGE ", 10) == 0) {
        char target[16]; if (sscanf(buf+10, "%15s", target) != 1) { send(fd, "ERR Usage: CHALLENGE <user>\n", 28, 0); return; }
        if (strcmp(target, clients[i].name) == 0) { send(fd, "ERR You cannot challenge yourself\n", 34, 0); return; }
        for (int j = 0; j < MAX_CLIENTS; j++)
            if (clients[j].fd != -1 && clients[j].loggedIn && strcmp(clients[j].name, target)==0) {
                char msg[64]; snprintf(msg, sizeof(msg), "CHALLENGE_FROM %s\n", clients[i].name);
                send(clients[j].fd, msg, strlen(msg), 0);
                send(fd, "OK Challenge sent\n", 18, 0); return;
            }
        send(fd, "ERR No such user\n", 17, 0);
    }
    else if (strncmp(buf, "REFUSE ", 7) == 0) {
        char opp[16]; if (sscanf(buf+7, "%15s", opp) != 1) { send(fd, "ERR Usage: REFUSE <user>\n", 25, 0); return; }
        for (int j = 0; j < MAX_CLIENTS; j++)
            if (clients[j].fd != -1 && strcmp(clients[j].name, opp)==0) {
                char msg[64]; snprintf(msg, sizeof(msg), "REFUSED_BY %s\n", clients[i].name);
                send(clients[j].fd, msg, strlen(msg), 0);
                send(fd, "OK Challenge refused\n", 22, 0); return;
            }
        send(fd, "ERR No such user\n", 17, 0);
    }
    else if (strncmp(buf, "ACCEPT ", 7) == 0) {
        char opp[16]; if (sscanf(buf+7, "%15s", opp) != 1) { send(fd, "ERR Usage: ACCEPT <user>\n", 25, 0); return; }
        for (int j = 0; j < MAX_CLIENTS; j++)
            if (clients[j].fd != -1 && strcmp(clients[j].name, opp)==0) {
                startGame(i, j); return;
            }
        send(fd, "ERR No such user\n", 17, 0);
    }
    else if (strncmp(buf, "READY", 5) == 0) {
        if (!clients[i].inGame) { send(fd, "ERR Not in game\n", 16, 0); return; }
        clients[i].ready = 1;
        int j = clients[i].opponent;
        if (j >= 0 && clients[j].ready) {
            for (int k = 0; k < MAX_GAMES; k++)
                if (games[k].active && (
                    strcmp(games[k].j0.pseudo, clients[i].name)==0 ||
                    strcmp(games[k].j1.pseudo, clients[i].name)==0))
                    sendBoard(&games[k]);
        }
    }
    else if (strncmp(buf, "MOVE ", 5) == 0) {
        int pit; if (sscanf(buf+5, "%d", &pit) != 1) { send(fd, "ERR Usage: MOVE <0-11>\n", 23, 0); return; }
        processMove(i, pit);
    }
    else if (strncmp(buf, "CANCEL_GAME", 11) == 0) {
        if (!clients[i].inGame) { send(fd, "ERR You are not in a game\n", 26, 0); return; }
        int opp = clients[i].opponent;
        if (opp >= 0 && clients[opp].fd != -1) {
            char msg[64]; snprintf(msg, sizeof(msg), "INFO %s canceled the game.\n", clients[i].name);
            send(clients[opp].fd, msg, strlen(msg), 0);
            clients[opp].inGame = 0; clients[opp].opponent = -1; clients[opp].ready = 0;
        }
        for (int g = 0; g < MAX_GAMES; g++)
            if (games[g].active && (
                strcmp(games[g].j0.pseudo, clients[i].name)==0 ||
                strcmp(games[g].j1.pseudo, clients[i].name)==0))
                games[g].active = 0;
        clients[i].inGame = 0; clients[i].opponent = -1; clients[i].ready = 0;
        send(fd, "OK Game canceled. Back to menu.\n", 32, 0);
    }
    else if (strncmp(buf, "MESSAGE ", 8) == 0) {
        char target[16], msgBody[480];
        if (sscanf(buf+8, "%15s %479[^\n]", target, msgBody) < 2) { send(fd, "ERR Usage: MESSAGE <user> <message>\n", 36, 0); return; }
        for (int j = 0; j < MAX_CLIENTS; j++)
            if (clients[j].fd != -1 && clients[j].loggedIn &&
                strcmp(clients[j].name, target)==0) {
                char pm[512];
                snprintf(pm, sizeof(pm), "PM from %s: %s\n", clients[i].name, msgBody);
                pm[sizeof(pm) - 1] = '\0';
                send(clients[j].fd, pm, strlen(pm), 0);
                send(fd, "OK Message sent\n", 16, 0); return;
            }
        send(fd, "ERR User not found\n", 19, 0);
    }
    else if (strncmp(buf, "SAY ", 4) == 0) {
        char msg[512];
        size_t msglen = strlen(buf + 4);
        if (msglen > 450) msglen = 450;  
        snprintf(msg, sizeof(msg), "CHAT %s: %.*s\n", clients[i].name, (int)msglen, buf+4);
        msg[sizeof(msg) - 1] = '\0';
        broadcast(msg, fd);
    }
    else if (strncmp(buf, "QUIT", 4) == 0) {
        removeClient(fd);
    }
    else {
        send(fd, "ERR Unknown command\n", 20, 0);
    }
}

int main() {
    mkdir("data", 0777);
    mkdir("games", 0777);
    accountCount = loadAccounts();
    printf("Loaded %d user accounts.\n", accountCount);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); exit(1);
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen"); close(server_fd); exit(1);
    }

    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    max_fd = server_fd;

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;
    for (int g = 0; g < MAX_GAMES; g++) games[g].active = 0;

    printf("Awalé server listening on port %d...\n", PORT);

    while (1) {
        fd_set read_fds = master_set;
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) { perror("select"); break; }
        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) handleNewConnection(server_fd);
                else handleClientMessage(i);
            }
        }
    }

    close(server_fd);
    return 0;
}