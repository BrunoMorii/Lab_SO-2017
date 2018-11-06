#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX 256

int main() {
	char comando[MAX], arg[MAX], tok[MAX];
	int pid;

	while (1){
		printf("> ");
		fgets(arg, MAX, stdin);
		if (!strcmp(comando, "exit")) {
			exit(EXIT_SUCCESS);
		}

		do{
			
		}while(tok != NULL)



		pid = fork();
		if (pid) {
				waitpid(pid, NULL, 0); 
		} else {
			execlp(comando, comando, NULL);
			printf("Erro ao executar comando!\n");
			exit(EXIT_FAILURE);
		}
	}
}
