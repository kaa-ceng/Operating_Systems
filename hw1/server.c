#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/wait.h>

#include "game_structs.h"
#include "print_output.h"

#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

/*This setup allows for bidirectional communication, where each end of the pipe can read and write
data.
The server must handle multiple players concurrently. It should use select() or poll() to check for
incoming data on the player pipes without blocking.
*/


/*
Server tasks:
read configuration from stdin
create pipes for each player
fork player processes
handle messages START, MARK
Send responses RESULT, END
manage game state and check win/draw
*/

/*
player can send start or mark
response can be result or end
check if the game is over after every possible move
*/

/*
use select or poll to monitor pipes.
these functions allow non-blocking checks for input
essential for real-time message handling
*/

/*
select:
monitors multiple file descriptors for readiness (data to read)
function signature: 
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
parameters:
nfds: highest file descriptor number + 1
readfds: set of file descriptors to monitor for reading
writefds: set for writing rediness (can be NULL)
exceptfds: set for exceptions (can be NULL)
timeout: maximum wait time (NULL for indefinite)

Understanding select - Code Example

* Example usage:
  fd_set readfds;
  struct timeval tv = {0, 1000}; // 1ms timeout
  FD_ZERO(&readfds);
  for (int i = 0; i < count; i++) {
    FD_SET(pipes[i], &readfds);
  }
  int ready = select(max_fd + 1, &readfds, NULL, NULL, &tv);
  if (ready > 0) {
    for (int i = 0; i < count; i++) {
      if (FD_ISSET(pipes[i], &readfds)) {
        // Read from pipe
      }
    }
  }
*/

//debug print grid
void printGrid(char** myGrid, int height, int width){
    printf("current grid state\n");
    for (int h=0; h <height; h++) {
        for (int w =0; w < width; w++) {
            printf("%c ", myGrid[h][w]);
        }
        printf("\n");
    }
    printf("\n");
}

int main() {
    

    // initialize game state from input
    int width, height, streak_size, player_count;
	scanf("%d %d %d %d", &width, &height, &streak_size, &player_count);

    //debug
    //printf("width:%d, height: %d, streak_size: %d, player_count:%d\n", width, height, streak_size, player_count);

    char** myGrid = malloc(height*sizeof(char*));
    for (int h=0; h<height; h++) {
        myGrid[h] = malloc(width);
        for (int w = 0; w < width; w++) {
            myGrid[h][w] = '.';
        }
    }


    //debug
    //printGrid(myGrid, height, width);

    typedef struct myPlayer {
        char character;
        int argCount;
        char **arguments;   //argument array
        int fd[2]; // socket, bidirectional pipe use
        int pid;
    } myPlayer;

    // creating array of players
    myPlayer* myPlayers = malloc(player_count*sizeof(myPlayer));
    for (int i=0; i < player_count; i++) {
        scanf(" %c %d", &myPlayers[i].character, &myPlayers[i].argCount);

        //dynamic allocation
        // zero index is executable path
        // last is null
        myPlayers[i].arguments = malloc((myPlayers[i].argCount+2)*sizeof(char*));
        
        // executable path input, size 200 should be more?
        myPlayers[i].arguments[0] = malloc(200);
        scanf("%s", myPlayers[i].arguments[0]);
        
        // taking arguments input
        for (int j=0; j < myPlayers[i].argCount; j++) {
            myPlayers[i].arguments[j+1] = malloc(200);
            scanf("%s", myPlayers[i].arguments[j+1]);
        }
        
        // last is null
        myPlayers[i].arguments[myPlayers[i].argCount+1] = NULL;
    }

    //debug
    /*for (int i=0; i<player_count; i++) {
        printf("player %d: character:%c, arguments:", i+1, myPlayers[i].character);
        for (int j=0; j< yPlayers[i].argCount + 1; j++) {
            printf("%s ", myPlayers[i].arguments[j]);
        }
        printf("\n");
    }*/
    //

    
    // Set up communication channels with players
    
    // create bidirectional pipes from main proccess to each player
    // in biredirectional pipes, both ends can read and write
    int pids[player_count];

    for (int i=0; i<player_count; i++) {  
        PIPE(myPlayers[i].fd);
        //debug
        //printf("Created bidirectional pipe for player %d: fd[0]=%d, fd[1]=%d\n", i + 1, myPlayers[i].fd[0], myPlayers[i].fd[1]);
    }

    
    //fork player processes and set up communication
    for (int i=0; i<player_count; i++) {
        pids[i] = fork();

        if (pids[i]>0){  //parent
            //printf("Created player %c process with PID: %d\n", myPlayers[i].character, pids[i]);
            
            // Store the player's PID
            myPlayers[i].pid = pids[i];
            
            // Close the child end of the pipe
            close(myPlayers[i].fd[1]);
            
            // Keep fd[0] open for communication with this player
        }

        else {
            //child process- player
            //printf("child process player %c started (PID: %d)\n", myPlayers[i].character, getpid());
            
            dup2(myPlayers[i].fd[1], 0);    // dup stdin to pipe write end
            dup2(myPlayers[i].fd[1], 1);    // dup stdout to pipe write end
            
            close(myPlayers[i].fd[0]);      // close read end (server)
            close(myPlayers[i].fd[1]);      // close dup2()ed end 
            
            
            if (execv(myPlayers[i].arguments[0], myPlayers[i].arguments)) {
				perror("execv");
				return -1;
			}
        }
    }

    
    // while game is not over do
        //Wait for player messages using select or poll
        //Process each incoming message
        //Update game state if the message is a valid mark
        //Check if the game is won or if the grid is full
        //Send appropriate responses to players
    //end while

    bool isGameOver = false;
    int filledCount = 0;
    char myWinner = '.';
    bool isDraw = false;

    // grid data to send current game state
    // sending filled posititon and caracter data
    //from game_structs.h
    //different for print_output
    gd *gridData = NULL;    //creating emty grid data. there is no marked position

    while(!isGameOver) {
        fd_set readfds;     // set of file descriptors to read
        int maxfd = 0;     // maximum numbered file descriptor
        
        /* setup for select() call. construct the set of file desriptors */
        FD_ZERO(&readfds);
        
        for (int i=0; i<player_count; i++) {
            FD_SET(myPlayers[i].fd[0], &readfds);
            if (myPlayers[i].fd[0] > maxfd) {
                maxfd = myPlayers[i].fd[0];
            }
        }

        struct timeval tv = {0, 1000}; // 1ms timeout

        /* our file descriptors block only for reading
           only define read set, set others to NULL */
		//select(maxfd+1, &readfds, NULL, NULL, NULL); 
        /* now, pset contains the descriptors that are ready.
		   collect and consume all ready file descriptors 
           more than one can be ready. so keep looping for all */

        // Wait for input on any player pipe
        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        if (ready > 0) {
            for (int i=   0; i <player_count; i++) {
                // Check if this player's pipe has data to read
                if (FD_ISSET(myPlayers[i].fd[0], &readfds)) { // this fd is ready to read
                    /* BLOCKING READ. BUT WE KNOW THAT THERE IS DATA */

                    // Read from pipe

                    int n;
                    cm myClientMessage;
                    n = read(myPlayers[i].fd[0], &myClientMessage, sizeof(cm));
                    
                    if (n <= 0) {
                        /*
                        active[i] = 0;
					    activecount--;
                        */
                    } else {

                        //message taken, print client message
                        cmp myClientPrint;
                        myClientPrint.process_id = myPlayers[i].pid;
                        myClientPrint.client_message = &myClientMessage;
                        print_output(&myClientPrint, NULL, NULL, 0);
                        
                        //debug
                        //printf("received message from player %c with pid: %d)\n", myPlayers[i].character, myPlayers[i].pid);
                        
                        //START: Sent when a player process is initialized. The server responds by sending the current state of the board.
                        if(myClientMessage.type == START){
                            
                            // server response
                            sm myServerMessage;
                            myServerMessage.type = RESULT;
                            myServerMessage.success = 0;            //there is no mark success
                            myServerMessage.filled_count = filledCount;

                            // since there is no success marking, i dont add anything to gridData

                            // server message
                            smp myServerPrint;
                            myServerPrint.process_id = myPlayers[i].pid;
                            myServerPrint.server_message = &myServerMessage;
                            
                            // print response
                            print_output(NULL, &myServerPrint, (gu *)gridData, filledCount);
                            
                            // server response message
                            write(myPlayers[i].fd[0], &myServerMessage, sizeof(sm));
                            write(myPlayers[i].fd[0], gridData, filledCount * sizeof(gd));
                            
                            //debug
                            // - printGrid(myGrid, height, width);
                        }


                        else if(myClientMessage.type == MARK){   
                            // checking mark position
                            int positionX = myClientMessage.position.x;
                            int positionY = myClientMessage.position.y;
                            // for a poisition to be marked it should be inside the grid
                            //  position should be empty = dots
                            if (myGrid[positionY][positionX] == '.' &&
                                positionX >= 0 && positionX < width &&
                                positionY >= 0 && positionY < height ) {

                                // mark position is empty
                                
                                // update myGrid with char
                                myGrid[positionY][positionX] = myPlayers[i].character;
                                filledCount++;

                                // adding character to grid data with the marked position
                                // so, i resize the gridData with filled countt
                                gd *newGridData = realloc(gridData, (filledCount)*sizeof(gd));
                                
                                gridData = newGridData;

                                //filling the grid data with the marked position
                                gridData[filledCount-1].position.x = positionX;
                                gridData[filledCount-1].position.y = positionY;
                                gridData[filledCount-1].character = myPlayers[i].character;
                                

                                //check draw
                                if(filledCount == (width*height)){
                                    //debug
                                    //printf("Draw\n");
                                    isGameOver = true;
                                    isDraw = true;
                                    
                                    // first send RESULT message then END
                                    sm myServerMessage;
                                    myServerMessage.type = RESULT;
                                    myServerMessage.success = 1;            //marked
                                    myServerMessage.filled_count = filledCount;

                                    // server message
                                    smp myServerPrint;
                                    myServerPrint.process_id = myPlayers[i].pid;
                                    myServerPrint.server_message = &myServerMessage;
                                    
                                    // print response
                                    print_output(NULL, &myServerPrint, (gu*)gridData, filledCount);
                                    
                                    // server response message
                                    write(myPlayers[i].fd[0], &myServerMessage, sizeof(sm));
                                    write(myPlayers[i].fd[0], gridData, filledCount * sizeof(gd));
                                    
                                    //debug
                                    //printGrid(myGrid, height, width);


                                    // sending END message to all players
                                    for(int endIndex=0; endIndex < player_count; endIndex++){
                                        
                                        sm myServerMessage;
                                        myServerMessage.type = END;
                                        myServerMessage.success = 1;    //mark successful
                                        myServerMessage.filled_count = 0;   //change filled count zero to terminate child

                                        // printing message
                                        smp myServerPrint;
                                        myServerPrint.process_id = myPlayers[endIndex].pid;
                                        myServerPrint.server_message = &myServerMessage;

                                        write(myPlayers[endIndex].fd[0], &myServerMessage, sizeof(sm));
                                        
                                        // print END message
                                        print_output(NULL, &myServerPrint, NULL, 0);
                                    }
                                    break;  //game over
                                    
                                }

                                else{
                                    // checking if a player won the game
                                    //checking using the last marked character and position
                                    int positionX = myClientMessage.position.x;
                                    int positionY = myClientMessage.position.y;
                                    char playerChar = myPlayers[i].character;   

                                    int currentStreak=1;    //i already marked one position
                                    
                                    //checking row win
                                    //position Y doesnt change
                                    //checking the left of the marked position
                                    for(int i = positionX-1; i >= 0; i--){
                                        if(myGrid[positionY][i] == playerChar){
                                            currentStreak++;
                                        }
                                        //there is no more streak to that side
                                        else{
                                            break;
                                        }
                                    }

                                    //checking the right of the marked position
                                    for(int i = positionX+1; i < width; i++){
                                        if(myGrid[positionY][i] == playerChar){
                                            currentStreak++;
                                        }
                                        else{
                                            break;
                                        }
                                    }
                                    if(currentStreak >= streak_size){
                                        myWinner = playerChar;
                                        isGameOver = true;
                                        //printf("row win\n");
                                    }

                                    //if there is no winner, check column
                                    if(myWinner == '.'){
                                        currentStreak = 1;
                                        //check above side of the position
                                        for(int i=positionY-1; i >= 0; i--){
                                            if(myGrid[i][positionX] == playerChar){
                                                currentStreak++;
                                            }
                                            else{
                                                break;
                                            }
                                        }
                                        //check below side of the position
                                        for(int i= positionY+1; i < height; i++){
                                            if(myGrid[i][positionX] == playerChar){
                                                currentStreak++;
                                            }
                                            else{
                                                break;
                                            }
                                        }
                                        if(currentStreak >= streak_size){
                                            myWinner = playerChar;
                                            isGameOver = true;
                                            //printf("column win\n");
                                        }
                                    }

                                    //if there is no winner, check diagonal lef up to right down
                                    if(myWinner == '.'){
                                        currentStreak = 1;
                                        //check left up side of the position
                                        int k = positionX-1;
                                        int l = positionY-1;
                                        while(k >= 0 && l >= 0){
                                            if(myGrid[l][k] == playerChar){
                                                currentStreak++;
                                            }
                                            else{
                                                break;
                                            }
                                            k--;
                                            l--;
                                        }
                                        //check right down side of the position
                                        k = positionX+1;
                                        l = positionY+1;
                                        while(k < width && l < height){
                                            if(myGrid[l][k] == playerChar){
                                                currentStreak++;
                                            }
                                            else{
                                                break;
                                            }
                                            k++;
                                            l++;
                                        }
                                        if(currentStreak >= streak_size){
                                            myWinner = playerChar;
                                            isGameOver = true;
                                            //printf("left top to bottom win\n");
                                        }
                                    }

                                    //if there is no winner, check diagonal left down to right up
                                    if(myWinner == '.'){
                                        currentStreak = 1;
                                        //check left down side of the position
                                        int k = positionX-1;
                                        int l = positionY+1;
                                        while(k >= 0 && l < height){
                                            if(myGrid[l][k] == playerChar){
                                                currentStreak++;
                                            }
                                            else{
                                                break;
                                            }
                                            k--;
                                            l++;
                                        }
                                        //check right up side of the position
                                        k = positionX+1;
                                        l = positionY-1;
                                        while(k < width && l >= 0){
                                            if(myGrid[l][k] == playerChar){
                                                currentStreak++;
                                            }
                                            else{
                                                break;
                                            }
                                            k++;
                                            l--;
                                        }
                                        if(currentStreak >= streak_size){
                                            myWinner = playerChar;
                                            isGameOver = true;
                                            //printf("left bottom to right top win\n");
                                        }
                                    }


                                    //if there is a winner, send RESULT and END message
                                    if(myWinner != '.'){
                                        // case for mark succesfull, no draw, yes win
                                        isGameOver = true;

                                        // first send RESULT message then END
                                        sm myServerMessage;
                                        myServerMessage.type = RESULT;
                                        myServerMessage.success = 1;            //marked
                                        myServerMessage.filled_count = filledCount;

                                        // server message
                                        smp myServerPrint;
                                        myServerPrint.process_id = myPlayers[i].pid;
                                        myServerPrint.server_message = &myServerMessage;
                                        
                                        // print response
                                        print_output(NULL, &myServerPrint, (gu *)gridData, filledCount);
                                        
                                        // server response message
                                        write(myPlayers[i].fd[0], &myServerMessage, sizeof(sm));
                                        write(myPlayers[i].fd[0], gridData, filledCount * sizeof(gd));
                                        
                                        //debug
                                        // - printGrid(myGrid, height, width);

                                        
                                        // END message to all players
                                        for(int endIndex = 0; endIndex < player_count; endIndex++){
                                        
                                            sm myServerMessage;
                                            myServerMessage.type = END;
                                            myServerMessage.success = 1;    //mark successful
                                            myServerMessage.filled_count = 0; // change filled count zero to terminate child

                                            // printing message
                                            smp myServerPrint;
                                            myServerPrint.process_id = myPlayers[endIndex].pid;
                                            myServerPrint.server_message = &myServerMessage;

                                            write(myPlayers[endIndex].fd[0], &myServerMessage, sizeof(sm));
                                            
                                            // print END message
                                            print_output(NULL, &myServerPrint, NULL, 0);
                                        }
                                        break;  //game over
                                    }

                                    //no winner, game continues
                                    else{
                                        // case for mark succesfull, no draw, no win
                                        sm myServerMessage;
                                        myServerMessage.type = RESULT;
                                        myServerMessage.success = 1;            //mark successful
                                        myServerMessage.filled_count = filledCount;

                                        // server msgsage
                                        smp myServerPrint;
                                        myServerPrint.process_id = myPlayers[i].pid;
                                        myServerPrint.server_message = &myServerMessage;
                                        
                                        // print response
                                        print_output(NULL, &myServerPrint, (gu *)gridData, filledCount);
                                        
                                        //sending response and gridData 
                                        write(myPlayers[i].fd[0], &myServerMessage, sizeof(sm));
                                        write(myPlayers[i].fd[0], gridData, filledCount * sizeof(gd));
                                    }
                                }          
                            } else {
                                //mark place is full already
                                sm myServerMessage;
                                myServerMessage.type = RESULT;
                                myServerMessage.success = 0;      //mark not successful
                                myServerMessage.filled_count = filledCount;

                                // server print
                                smp myServerPrint;
                                myServerPrint.process_id = myPlayers[i].pid;
                                myServerPrint.server_message = &myServerMessage;
                                
                                // print response
                                print_output(NULL, &myServerPrint, (gu *)gridData, filledCount);
                                
                                //sending response and gridData 
                                write(myPlayers[i].fd[0], &myServerMessage, sizeof(sm));
                                write(myPlayers[i].fd[0], gridData, filledCount * sizeof(gd));
                            }
                        }
                    }
                }
            }
        }
    }


    //debug
    //printGrid(myGrid, height, width);

    // Announce the winner or declare a draw
    if(isDraw){
        printf("Draw\n");
    }

    if(myWinner != '.'){
        printf("Winner: Player%c\n", myWinner); // For a win
    }



    // Clean up resources and terminate

    //free myGrid - 2d dimensions
    for (int i = 0; i < height; i++) {
        free(myGrid[i]);
    }
    free(myGrid);

    // free arguments of players
    for (int i = 0; i < player_count; i++) {
        free(myPlayers[i].arguments[0]);
        for (int j = 1; j < myPlayers[i].argCount + 1; j++) {
            free(myPlayers[i].arguments[j]);
        }
        free(myPlayers[i].arguments);
        
        //debug
        //printf("free memory of player %d\n", i + 1);
    }

    //closing pipes
    for (int i = 0; i < player_count; i++) {
        close(myPlayers[i].fd[0]);
        close(myPlayers[i].fd[1]);

        //debug
        //printf("closed pipe to player %d\n", i + 1);
    }

    free(myPlayers);
    //printf("free memory for players\n");

    free(gridData);
    //printf("free memory for gridData\n");
    
    
    // The server must reap all child processes to avoid zombie processes.

    for (int i = 0; i < player_count; i++) {
        //waiting child to terminate
        int status;
        waitpid(pids[i], &status, 0);
    }
    //printf("end of the program\n");



    return 0;
}