


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXNAME 80  /* maximum permitted name size, not including \0 */
#define NPITS 6  /* number of pits on a side, not including the end pit */
#define NPEBBLES 4 /* initial number of pebbles per pit */
#define MAXMESSAGE (MAXNAME + 50) /* initial number of pebbles per pit */
#define EMPTY_NAME_MESSAGE "Empty name. Enter your name again\r\n"
#define LONG_NAME_MESSAGE "Name too long. Choose another one and reconnect again\r\n"
#define REPEAT_NAME_MESSAGE "Name has been taken. Choose another one and reconnect again\r\n"

int port = 3000;
int listenfd;

struct player {
    int fd;
    char name[MAXNAME+1];
    int pits[NPITS+1];  // pits[0..NPITS-1] are the regular pits
    // pits[NPITS] is the end pit
    //other stuff undoubtedly needed here
    int turn; // If it is this player's turn, turn is 0. Otherwise, turn is 1.
    int waiting_for_username; // If the server finishes reading this player's name, waiting_for_username is 0. Otherwise, it is 1.
    struct player *next;
};
struct player *playerlist = NULL;

// Helper function for the error checking of write
void Write(int fd, char *buf, size_t nbytes){
    if (write(fd, buf, nbytes) < 0){
        perror("write");
        exit(1);  
    }
}

extern void parseargs(int argc, char **argv);
extern void makelistener();
extern int compute_average_pebbles();
extern int game_is_over();
extern void broadcast(char *s);
extern int accept_connection(int fd);
extern void display_game(struct player *if_all_player);
extern int read_full_name(struct player *set_player);
extern void disconnect(struct player *disconnect_player);
extern int start_move(struct player *move_player);
extern struct player *find_next_player(struct player *current_player);
extern struct player *find_current_player();
extern int find_network_newline(const char *buf, int n);
extern void ask_move(struct player *move_player);
extern void broadcastother(char *s, struct player *current_player);

int main(int argc, char **argv) {
    char msg[MAXMESSAGE];

    parseargs(argc, argv);
    makelistener();

    int max_fd = listenfd;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(listenfd, &all_fds);
    struct player *current_turn_player; // a pointer for each turn's player

    while (!game_is_over()){
        int first_player = 1; // If current turn's player is the first player, it is 0. Otherwise, it is 1.
        int ask_to_move = 1; // If current player has successfully made a move, it is 0. Otherwise, it is 1.
        int display = 0; //If we need to display the game status, it is 0. Otherwise, it is 1. 

        fd_set listen_fds = all_fds;

        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (nready == -1) {
            perror("server: select");
            exit(1);
        }

        // There is a new player coming in.
        if (FD_ISSET(listenfd, &listen_fds)){
            int client_fd = accept_connection(listenfd);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("Accepted connection from a new player\n");
        }

        struct player *free_player = NULL; // The player needed to free memory on heap.
        int free_flag = 1; // If we have already freed a player, it is 0. Otherwise, it is 1.
        struct player *p = playerlist;
        while (p != NULL){
            if (FD_ISSET(p -> fd, &listen_fds)){ // Select all activated players.
                if(p -> waiting_for_username != 0){ // If the server does not finish reading p's name, continue to read
                    int check_name_valid = 
                    read_full_name(p);
                    if (check_name_valid == 0){ // Server successfully finishes reading p'name

                        char new_player_board[MAXMESSAGE + 1];
                        sprintf(new_player_board, "%s joined the game\n", p -> name);
                        broadcast(new_player_board);

                        char new_player_message[MAXMESSAGE + 1] = "Current board: \r\n";
                        broadcast(new_player_message);
                        display_game(NULL);

                        if (current_turn_player == NULL){ // If p is the first player
                            p -> turn = 0;
                            ask_move(p);
                            first_player = 0;
                        } else{
                            char broadcast_message[MAXMESSAGE];
                            sprintf(broadcast_message, "It is %s's move\r\n", current_turn_player -> name);
                            Write(p -> fd, broadcast_message, strlen(broadcast_message));
                            char move_message[MAXMESSAGE + 1] = "Your move?\r\n";
                            Write(current_turn_player -> fd, move_message, strlen(move_message));
                        }
                        printf("%s entered name and joined the game\n", p -> name);
                    } else if (check_name_valid == 2){ // p is disconnected
                         disconnect(p);
                         FD_CLR(p -> fd, &all_fds);
                         close(p -> fd);
                         free_player = p;
                    }
                } else{
                    if(p != current_turn_player){ // It is not p's turn, but p types something
                        int check_move_status = start_move(p);
                        if (check_move_status == 4){ // Successfully reads p's input
                            char *warn_message = "It's not your move\r\n";
                            Write(p -> fd, warn_message, strlen(warn_message));
                        } else { 
                            ask_to_move = 0;
                            if (check_move_status == 2){ // p is disconnected
                                disconnect(p);
                                FD_CLR(p -> fd, &all_fds);
                                close(p -> fd);
                                free_player = p;
                            }
                        }
                    } else{
                        int check_next_player = start_move(p);
                        ask_to_move = 0;
                        if (check_next_player == 1){ // Find next turn's player and updates p's turn and next player's turn
                            struct player *next_player = find_next_player(p);
                            if (p != next_player){
                                next_player -> turn = 0;
                                p -> turn = 1;
                            }
                            printf("%s made a move\n", p -> name);
                        } else if (check_next_player == 0){ // p moves pebbles into his end pit, so we don't need to update next turn
                            printf("%s made a move\n", p -> name);
                        } else if (check_next_player == 2){ // p is disconnected
                            FD_CLR(p -> fd, &all_fds);
                            close(p -> fd);
                            disconnect(p);
                            free_player = p;
                        } else if (check_next_player == 3){ // p's input is not a valid pit number
                            display = 1;
                        }
                    }
                }
            }
            p = p -> next;

            if ((free_player != NULL) && (free_flag == 1)){ // Free the disconnected player.
                memset(&free_player -> name[0], '\0', MAXNAME + 1);
                free(free_player);
                free_flag = 0;
            }
        }

        current_turn_player = find_current_player(); // Find and current turn's player after one turn.
        if ((current_turn_player != NULL) && (first_player == 1) && (ask_to_move == 0)){
            if (display == 0){
                char *after_move_message = "Current board: \r\n";
                broadcast(after_move_message);
                display_game(NULL);
            }
            if (!game_is_over()){
                if (display == 0){
                    ask_move(current_turn_player);
                } else{
                    char move_message[MAXMESSAGE + 1] = "Your move?\r\n";
                    Write(current_turn_player -> fd, move_message, strlen(move_message));
                }
            }
        }
    }

    broadcast("Game over!\r\n");
    printf("Game over!\n");
    for (struct player *p = playerlist; p; p = p->next) {
        int points = 0;
        for (int i = 0; i <= NPITS; i++) {
            points += p->pits[i];
        }
        printf("%s has %d points\r\n", p->name, points);
        snprintf(msg, MAXMESSAGE, "%s has %d points\r\n", p->name, points);
        broadcast(msg);
    }
    return 0;
}

/* Return current turn's player after one turn */
struct player *find_current_player(){
    for (struct player *p = playerlist; p; p = p->next) {
        if (p -> turn == 0){
            return p;
        }
    }
    return NULL;
}

/* Ask move_player to move and tell everyone else it is move_player's move */
void ask_move(struct player *move_player){
    char move_message[MAXMESSAGE + 1] = "Your move?\r\n";
    Write(move_player -> fd, move_message, strlen(move_message));

    char broadcast_message[MAXMESSAGE];
    sprintf(broadcast_message, "It is %s's move\r\n", move_player -> name);
    broadcastother(broadcast_message, move_player);
}

/* Return current_player's next player whose name has been finished reading from server */
struct player *find_next_player(struct player *current_player){
    // Starts looping from current_player until the end of playerlist.
    for (struct player *p = current_player -> next; p; p = p->next){
        if (p -> waiting_for_username == 0){
            return p;
        }
    }

    // Starts looping from the head of playerlist until the current_player.
    for (struct player *p = playerlist; p != current_player -> next && p; p = p -> next){
        if (p -> waiting_for_username == 0){
            return p;
        }
    }

    // Cannot find next player whose name has been finished reading from server.
    return NULL;
}

/* move_player is trying to make a move
Return 0 if it is still move_player's turn, we don't need to update turn
Return 1 if we need to find next player and updates move_player's turn and next player's turn
Return 2 means move_player is disconnected
Return 3 means it is move_player's turn and his input is not a valid pit
Return 4 means it is not move_player's turn
*/
int start_move(struct player *move_player){
    char buf[1];
    buf[0] = '\0';
    int num_read;
    char all_input[MAXMESSAGE + 1];
    int room = sizeof(all_input);
    all_input[0] = '\0';
    int inbuf = 0;

    while (room != 0){
        if (inbuf != 0){
            if (all_input[inbuf - 1] == '\n'){ // Read until find a new line character
                break;
            }
        }
        num_read = read(move_player -> fd, buf, sizeof(char));
        if (num_read == 0){
            return 2;
        } else if (num_read < 0){
            perror("read");
            exit(1);
        }
        inbuf += num_read;
        strncat(all_input, buf, sizeof(char));
        room --;
    }

    // Not move_player's turn
    if (move_player -> turn != 0){
        return 4;
    }

    // Read a blank line
    if ((all_input[0] == '\r') || (all_input[0] == '\n')){
        char invalid[MAXMESSAGE + 1] = "Invalid pit\r\n";
        Write(move_player -> fd, invalid, strlen(invalid));
        return 3;
    }
    int move_pit = strtol(all_input, NULL, 10);

    //If it is move_player's turn
    if ((move_pit < NPITS) && (move_pit >= 0) && (move_player -> pits[move_pit] > 0)){ // If the input is a valid pit
        char broad_move_message[MAXMESSAGE + 1];
        sprintf(broad_move_message, "%s moved pit #%d\r\n", move_player -> name, move_pit);
        broadcast(broad_move_message);

        int number_pebble = move_player -> pits[move_pit]; // Get the initial pebbles in move_player's pit
        move_player -> pits[move_pit] = 0; // Reset the pebbles in move_player's pit to 0
        struct player *current = move_player;
        move_pit += 1; // Starts to fill next pit

        while (number_pebble != 0){
            number_pebble -= 1;
            if (current == move_player){ // If the pebble still moves into move_player's board.
                if (move_pit == NPITS){ // If the pebble moves into the end pit
                    current -> pits[move_pit] += 1;
                    move_pit = 0;
                    if (number_pebble == 0){ // If it is the last pebble moves into the end pit, it is still move_player's turn
                        return 0;
                    } else{ // Else, we need to find next player's board
                        current = find_next_player(current);
                    }
                } else { // If the pebble does not move into the end pit, just increaments the pebbles in move_pit
                    current -> pits[move_pit] += 1;
                    move_pit += 1;
                }
            } else{ // If the pebble moves into other player's board.
                if (move_pit == NPITS - 1){ // If the pebble moves into the last pit before end pit, we need to find next player's board
                    current -> pits[move_pit] += 1;
                    move_pit = 0;
                    current = find_next_player(current);
                } else{ // Else, just increaments the pebbles in move_pit
                    current -> pits[move_pit] += 1;
                    move_pit += 1;
                }
            }
        }
        return 1;
    } else{ // If the input is not a valid pit
        char invalid[MAXMESSAGE + 1] = "Invalid pit\r\n";
        Write(move_player -> fd, invalid, strlen(invalid));
        return 3;
    }
}

/* If if_all_player is NULL, display the current board status to all players.
Otherwise, display the current board status only to if_all_player */
void display_game(struct player *if_all_player){
    for (struct player *p = playerlist; p; p = p->next) {
        char game_status[MAXMESSAGE];
        if (p -> waiting_for_username == 0){
            sprintf(game_status, "%s: ", p -> name);
            for (int i = 0; i <= NPITS - 1; i++){
                char pits_status[MAXMESSAGE];
                sprintf(pits_status, "[%d]%d ", i, p -> pits[i]);
                strcat(game_status, pits_status);
            }
            char end_pit_status[MAXMESSAGE];
            sprintf(end_pit_status, "[%s]%d\r\n", "end pit", p -> pits[NPITS]);
            strcat(game_status, end_pit_status);
            if (if_all_player == NULL){
                broadcast(game_status);
            } else{
                Write(if_all_player -> fd, game_status, strlen(game_status));
            }
        }
    }
}

/* Disconnect disconnect_player. Removes disconnect_player from playerlist and updates playerlist */
void disconnect(struct player *disconnect_player){
    if (disconnect_player -> waiting_for_username == 0){
        printf("%s is disconnected\n", disconnect_player -> name);
        char disconnect_message[MAXMESSAGE + 1];
        sprintf(disconnect_message, "%s is disconnected\r\n", disconnect_player -> name);
        broadcastother(disconnect_message, disconnect_player);
    } 

    struct player *head = playerlist;
    if ((disconnect_player == head) && (disconnect_player -> next == NULL)){ // Playerlist only contains disconnect_player
        playerlist = NULL;
    } else if (disconnect_player == head){ // disconnect_player is the head of linked list
        playerlist = disconnect_player -> next;
        if (disconnect_player -> turn == 0){ // If disconnect_player is current turn's player, we need to find next valid player and updates his turn.
            struct player *next_player = find_next_player(disconnect_player);
            if (next_player != NULL){
                next_player -> turn = 0;
            }
        }
    } else{ // disconnect_player is in the middle of linked list
        struct player *current = head;
        while (current -> next != disconnect_player){ // Find the player right before disconnect_player
            current = current -> next;
        }
        current -> next =  current -> next -> next; // Updates playerlist
        if (disconnect_player -> turn == 0){ // If disconnect_player is current turn's player, we need to find next valid player and updates his turn.
            struct player *next_player = find_next_player(disconnect_player);
            if (next_player != NULL){
                next_player -> turn = 0;
            }
        }
    }
}

/* Return the index in buf of length n where the network newline appears*/
int find_network_newline(const char *buf, int n) {
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\r' || buf[i] == '\n'){
            return i;
        }
    }
    return -1;
}

/* The server reads the set_player's name.
Return 0 if successfully finishes reading set_player's name which is valid.
Return 1 if set_player's name is empty.
Return 2 if set_player's name is too long or already taken by another one, and we disconnect set_player.
*/
int read_full_name(struct player *set_player){
    int flag = 1; // If the server finishes reading valid name of set_player, it is 0. Otherwise, it is 1.
    char copy_name[MAXNAME + 3]; // A variable to store partial name of set_player -> name
    memset(&copy_name[0], '\0', MAXNAME + 3);
    if (strlen(set_player -> name) != 0){ // If the server has read part of set_player's name
        strcpy(copy_name, set_player -> name); // Copy the set_player's name to copy_name
        copy_name[strlen(set_player -> name)] = '\0';
    } else{
        copy_name[0] = '\0';
    }
    int already_read = strlen(set_player -> name);
    int room = sizeof(copy_name) - already_read - 1;

    char buf[MAXNAME + 3]; // A buf to read and store the partial name of set_player.
    memset(&buf[0], '\0', MAXNAME + 3);
    int inbuf = read(set_player -> fd, buf, sizeof(buf));
    if (inbuf < 0){
        perror("read");
        exit(1);
    } else if (inbuf == 0){
        printf("Some player is disconnected\n");
        return 2;
    }
    int where = find_network_newline(buf, inbuf); // Find the index of network newline in buf if it exists
    strncat(copy_name, buf, room); // Strncat this turn's read name from buf into copy_name

    if (where == -1){ // If we didn't find the newline
        if (strlen(copy_name) <= MAXNAME){ // If the length of copy_name does not exceed MAXNAME
            strcpy(set_player -> name, copy_name); // Copy copy_name into set_player -> name
            set_player -> name[strlen(set_player -> name)] = '\0';
        } else{ // If the length of copy_name exceeds 80 chars, we disconnect set_player
            Write(set_player -> fd, LONG_NAME_MESSAGE, strlen(LONG_NAME_MESSAGE));
            printf("Some player is disconnected\n");
            return 2;
        }
    } else{ // If we found a newline
        where = already_read + where; // Find the index of network newline in copy_name
        if (where > MAXNAME){ // If the index of newline in copy_name exceeds MAXNAME, name is too long and we disconnect set_player 
            Write(set_player -> fd, LONG_NAME_MESSAGE, strlen(LONG_NAME_MESSAGE));
            printf("Some player is disconnected\n");
            return 2;
        } else{ // The index of newline in copy_name is valid, so we finishes reading name
            copy_name[where] = '\0';
            flag = 0;
        }
    }

    if (flag == 0){// If we successfully found the network newline in a valid range
        if (strlen(copy_name) == 0){ // If the set_player enters a blank line, asks him to enter again
            memset(&set_player -> name[0], '\0', MAXNAME + 1);
            Write(set_player -> fd, EMPTY_NAME_MESSAGE, strlen(EMPTY_NAME_MESSAGE));
            return 1;
        } 

        for (struct player *p = playerlist; p; p = p->next) {
            if (p -> waiting_for_username == 0){
                if (strcmp(p -> name, copy_name) == 0){ //If the set_player's name is equal to any existing player's name, disconnect him.
                    memset(&set_player -> name[0], '\0', MAXNAME + 1);
                    Write(set_player -> fd, REPEAT_NAME_MESSAGE, strlen(REPEAT_NAME_MESSAGE));
                    printf("Some player is disconnected\n");
                    return 2;
                }
            }
        }

        // Name is valid
        strcpy(set_player -> name, copy_name);
        set_player -> name[strlen(set_player -> name)] = '\0';
        set_player -> waiting_for_username = 0;
        return 0;
    }
    return -1;
}

/* Accept a new connection from a player given the server's socket fd. 
Return the fd to communicate with new player */
int accept_connection(int fd) {
    struct player *new_player = malloc(sizeof(struct player));
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    // Initialize new_player's each attributes
    new_player -> fd = client_fd;
    new_player -> turn = 1;
    new_player -> waiting_for_username = 1;
    memset(&new_player -> name[0], '\0', MAXNAME + 1);
    int average_pebbles = compute_average_pebbles();
    for (int j = 0; j < NPITS; j++){
        new_player -> pits[j] = average_pebbles;
    }
    new_player -> pits[NPITS] = 0;
    new_player -> next = playerlist;
    // Updates playerlist
    playerlist = new_player;

    // Asks new_player to enter their name.
    char *welcome_message = "Welcome to Mancala. What is your name?\r\n";
    Write(client_fd, welcome_message, strlen(welcome_message));
    return client_fd;
}

void parseargs(int argc, char **argv) {
    int c, status = 0;
    while ((c = getopt(argc, argv, "p:")) != EOF) {
        switch (c) {
            case 'p':
                port = strtol(optarg, NULL, 0);
                break;
            default:
                status++;
        }
    }
    if (status || optind != argc) {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        exit(1);
    }
}

void makelistener() {
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int on = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const char *) &on, sizeof(on)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
}

/* call this BEFORE linking the new player in to the list */
int compute_average_pebbles() {
    struct player *p;
    int i;

    if (playerlist == NULL) {
        return NPEBBLES;
    }

    int nplayers = 0, npebbles = 0;
    for (p = playerlist; p; p = p->next) {
        nplayers++;
        for (i = 0; i < NPITS; i++) {
            npebbles += p->pits[i];
        }
    }
    return ((npebbles - 1) / nplayers / NPITS + 1);  /* round up */
}

int game_is_over() { /* boolean */
    int i;

    if (!playerlist) {
        return 0;  /* we haven't even started yet! */
    }

    for (struct player *p = playerlist; p; p = p->next) {
        int is_all_empty = 1;
        for (i = 0; i < NPITS; i++) {
            if (p->pits[i]) {
                is_all_empty = 0;
            }
        }
        if (is_all_empty) {
            return 1;
        }
    }
    return 0;
}

/* Broadcast message to all players whose name has been finished reading from the server. */
void broadcast(char *message){
    for (struct player *p = playerlist; p; p = p->next) {
        if (p -> waiting_for_username == 0){
            Write(p -> fd, message, strlen(message));
        }
    }
}

/* Broadcast message to all players whose name has been finished reading from the server except from current_player. */
void broadcastother(char *message, struct player *current_player){
    for (struct player *p = playerlist; p; p = p->next) {
        if ((p -> waiting_for_username == 0) && (p != current_player)){
            Write(p -> fd, message, strlen(message));
        }
    }
}