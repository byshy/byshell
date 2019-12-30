#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <termios.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>

#define READ 0
#define WRITE 1

#define clear() printf("\033[H\033[J")
#define cursorforward(x) printf("\033[%dC", (x))
#define cursorbackward(x) printf("\033[%dD", (x))
#define backspace(x) printf("\033[%dP", (x))

#define KEY_ESCAPE  0x001b
#define KEY_ENTER   0x000a
#define KEY_UP      0x0105
#define KEY_DOWN    0x0106
#define KEY_RIGHT   0x0107
#define KEY_LEFT    0x0108
#define BACK_SPACE  0x007F

static struct termios term, oterm;

char currentPath[1024];

int exists(const char* filename){
    struct stat buffer;
    int exist = stat(filename,&buffer);
    if(exist == 0)
        return 1;
    else
        return 0;
}

void perform_command(char* command_token){
	int argscounter = 0;
    char* args[11];
	while (command_token != NULL && argscounter < 10) {
		args[argscounter] = command_token;
		command_token = strtok(NULL, " ");
		argscounter ++;
	}
	args[argscounter] = NULL;

	bool executed = false;	// or true
	bool found = executed;

	if(strcmp(args[0],"cd") == 0){
		chdir(args[1]);
		getcwd(currentPath, sizeof(currentPath));
	} else if (strcmp(args[0],"exit") == 0){
		exit(0);
	} else if (strcmp(args[0],"clear") == 0){
		clear();
	} else {
		char *dup = strdup(getenv("PATH"));
		char *s = dup;
		char *p = NULL;
		do {
		    p = strchr(s, ':');
		    if (p != NULL) {
		        p[0] = 0;
		    }
	
			char path[80];
			path[0] = 0;
			strcat(path,s);
			strcat(path,"/");
			strcat(path, args[0]);

			executed = true;
			if(exists(path) == 1){
				found = true;
				pid_t currentFork = fork();
	
				wait(NULL);
				if(currentFork == 0){
					int status = execv(path, args);
					if(status == -1){
						executed = false;
					}
					break;
				} else if(currentFork == -1){
					executed = false;
				}
				break;
			} else {
				found = false;
				executed = false;
			}
			    s = p + 1;
		} while (p != NULL);
			
		if(!found){
			printf("command not found\n");
		}

		if(!executed && found){
			printf("problem executing command\n");
		}
			
		free(dup);
	}
}

char *trimwhitespace(char *str){
  char *end;

  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)
    return str;

  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';

  return str;
}

static int getch(void){
    int c = 0;

    tcgetattr(0, &oterm);
    memcpy(&term, &oterm, sizeof(term));
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &term);
    c = getchar();
    tcsetattr(0, TCSANOW, &oterm);
    return c;
}

static int kbhit(void){
    int c = 0;

    tcgetattr(0, &oterm);
    memcpy(&term, &oterm, sizeof(term));
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 1;
    tcsetattr(0, TCSANOW, &term);
    c = getchar();
    tcsetattr(0, TCSANOW, &oterm);
    if (c != -1) ungetc(c, stdin);
    return ((c != -1) ? 1 : 0);
}

static int kbesc(void){
    int c;

    if (!kbhit()) return KEY_ESCAPE;
    c = getch();
    if (c == '[') {
        switch (getch()) {
            case 'A':
                c = KEY_UP;
                break;
            case 'B':
                c = KEY_DOWN;
                break;
            case 'C':
                c = KEY_RIGHT;
                break;
            case 'D':
                c = KEY_LEFT;
                break;
			case 'P':
				c = BACK_SPACE;
                break;
            default:
                c = 0;
                break;
        }
    } else {
        c = 0;
    }
    if (c == 0) while (kbhit()) getch();
    return c;
}

static int kbget(void){
    int c;

    c = getch();
    return (c == KEY_ESCAPE) ? kbesc() : c;
}

int main (int argc, char *argv[]){

	signal(SIGINT, SIG_IGN);

	char history[100][100];
	int history_index = 0;

	clear();

	getcwd(currentPath, sizeof(currentPath));
	
	int c;
    while (1) {
        printf ("\033[1;36m%s\033[0m> ", currentPath);

		char command[100];
		int com_index = 0;
		int cursor_index = 0;
		int history_pointer = history_index;

        while(1){
			c = kbget();
			if (c == KEY_ESCAPE) {
	            break;
        	} else if (c == BACK_SPACE){
				if(com_index != 0){ // this section contains a bug
									// try deleting a character within
									// a string and you'll see the error
		            cursorbackward(1);
					backspace(1);
					com_index--;
					cursor_index--;
					command[com_index] = '\0';
				}
				continue;
			} else if (c == KEY_ENTER){
				command[com_index] = '\0';
				printf("\n");
				break;
			} else if (c == KEY_UP){
				if(history_pointer > 0){
					history_pointer--;
					putchar(' ');
					com_index++;
					cursorbackward(cursor_index + 1);
					backspace(com_index);
					com_index = 0;
					cursor_index = 0;
					int length = 0;
					for(int i = 0; i < sizeof(history[history_pointer]); i++){
						if (history[history_pointer][i] != 0){
							length++;
						}
					}
					for(int i = 0; i < length; i++){
						putchar(history[history_pointer][i]);
						command[com_index] = history[history_pointer][i];
						cursor_index++;
						com_index++;
					}
				}
			} else if (c == KEY_DOWN){
				if(history_pointer < history_index){
					history_pointer++;
					putchar(' ');
					com_index++;
					cursorbackward(cursor_index + 1);
					backspace(com_index);
					com_index = 0;
					cursor_index = 0;
					int length = 0;
					for(int i = 0; i < sizeof(history[history_pointer]); i++){
						if (history[history_pointer][i] != 0){
							length++;
						}
					}
					for(int i = 0; i < length; i++){
						putchar(history[history_pointer][i]);
						command[com_index] = history[history_pointer][i];
						cursor_index++;
						com_index++;
					}
				}
			} else if (c == KEY_RIGHT) {
				if(cursor_index < com_index){
					cursor_index++;
					cursorforward(1);
				}
	        } else if (c == KEY_LEFT) {
				if(cursor_index > 0){
					cursor_index--;
		            cursorbackward(1);
				}
	        } else {
	            if(com_index < 100){
					putchar(c);

					if(cursor_index < com_index){	
						int diff = com_index - cursor_index;
						int index = cursor_index;
						char temp = c;
						while(com_index + 1 != index){
							char in_temp = command[index];
							command[index] = temp;
							temp = in_temp;
							if(index < com_index){
								putchar(temp);
							}
							index++;
						}
						cursorbackward(diff);
					} else {
						command[com_index] = c;
					}
	
					cursor_index++;
					com_index++;
				}
	        }
		}

		if(strcmp(command, "") == 0){
			continue;
		}

		strcpy(history[history_index], command);
		history_index++;

		const char* ptr = index(command, '>');	
		if(ptr != NULL){
			char* pipe_token = strtok(command, ">");
			pipe_token = trimwhitespace(pipe_token);
			char* pipe_token2 = strtok(NULL, ">");
			pipe_token2 = trimwhitespace(pipe_token2);
			
			if (pipe_token == NULL){
				printf("log: pipe_token is null\n");
			}
			
			int pid, fd[2];
			
			if ((pid = fork()) < 0){
				perror("fork failed");
				exit(2);
			} else if (pid == 0){
				if (pipe(fd) == -1){
					perror("pipe failed");
					exit(1);
				}

				int pid2 = fork();
				wait(NULL);
				if(pid2 == 0){
					close(fd[READ]);
					dup2(fd[WRITE], 1) ;
					close(fd[WRITE]); 
					execlp (pipe_token,pipe_token,NULL);
				} else if (pid2 > 0){
					close(fd[WRITE]);
					dup2(fd[READ], 0);
					close(fd[READ]);
					execlp (pipe_token2,pipe_token2,NULL);
				}
			}
			wait(NULL);
			continue;
		}

        char* command_token = strtok(command, " ");
		
        perform_command(command_token);
    }
    return 0;
}