#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

typedef enum state {
    Undefined,
    // TODO: Add additional states as necessary
    HELOexist,
    MAILexist,
    RCPTexist,
} State;

typedef struct smtp_state {
    int fd;
    net_buffer_t nb;
    char recvbuf[MAX_LINE_LENGTH + 1];
    char *words[MAX_LINE_LENGTH];
    int nwords;
    State state;
    struct utsname my_uname;
    // TODO: Add additional fields as necessary
    user_list_t userList;
} smtp_state;
    
static void handle_client(int fd);

int main(int argc, char *argv[]) {
  
    if (argc != 2) {
	fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
	return 1;
    }
  
    run_server(argv[1], handle_client);
  
    return 0;
}

// syntax_error returns
//   -1 if the server should exit
//    1  otherwise
int syntax_error(smtp_state *ms) {
    if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
    return 1;
    // comment to TA: I tried to use this helper function but it leads to the prairie Learn test not pass,
    // so I ended up with writing this two lines myself.
}

// checkstate returns
//   -1 if the server should exit
//    0 if the server is in the appropriate state
//    1 if the server is not in the appropriate state
int checkstate(smtp_state *ms, State s) {
    if (ms->state != s) {
	if (send_formatted(ms->fd, "503 %s\r\n", "Bad sequence of commands") <= 0) return -1;
	return 1;
    }
    return 0;
}

// isValidDomain returns
//   1 if it is a valid domain
//   0 if it isn't
int isValidDomain(char *str) {
    int len = strlen(str);

    for (int i = 0; i < len; i++) {
        if (str[i] < '0' && str[i] > '9' &&
            str[i] < 'a' && str[i] > 'z' &&
            str[i] < 'A' && str[i] > 'Z' &&
            str[i] != '.') {
            return 0;
        }
        if (str[i - 1] == '.' && str[i] == '.')
            return 0;
    }
    return 1;
}

// isValidPath checks whether there is at least one
//  character between <>, and returns
//   1 if it is a valid path
//   0 if it isn't
int isValidPath(char *str){
     int len= strlen(str);
     if(str[0] !='<' || str[len-1] !='>' || len < 3){
         return 0;
     }
    return 1;
}


// All the functions that implement a single command return
//   -1 if the server should exit
//    0 if the command was successful
//    1 if the command was unsuccessful

int do_quit(smtp_state *ms) {
    dlog("Executing quit\n");
    // TODO: Implement this function
    if(send_formatted(ms->fd, "221 %s\r\n", "Service closing transmission channel") <= 0) return -1;
    return -1;
}

int do_helo(smtp_state *ms) {
    dlog("Executing helo\n");
    // TODO: Implement this function
    if(send_formatted(ms->fd,"250 %s\r\n", ms->my_uname.nodename)<= 0) return -1;
    ms->state = HELOexist;
    return 0;
}

int do_rset(smtp_state *ms) {
    dlog("Executing rset\n");
    // TODO: Implement this function
//    Any stored sender, recipients, and mail data MUST be
//   discarded, and all buffers and state tables cleared.
    if(ms->nwords != 1){
        if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }
    ms->recvbuf[0] = '\0';
    if(ms->state == Undefined) {
        ms->state = Undefined;
    } else {
        ms->state = HELOexist;
    }
    if(ms->userList != NULL){
        user_list_destroy(ms->userList);
        ms->userList = NULL;
    }
//    mail_list_destroy(ms->mailItem);
    if(send_formatted(ms->fd,"250 State reset\r\n") <= 0) return -1;
    return 0;
}

int do_mail(smtp_state *ms) {
    dlog("Executing mail\n");
    // TODO: Implement this function
    int state;

    if(ms->nwords != 2 && ms->nwords != 3){
        if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    // Check whether the command starts with "MAIL FROM:<"
    if(strncasecmp(ms->words[1], "FROM:<", 6)){
        if(send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    char *start = strchr(ms->words[1],'<');  // find the end of the email address
    char *end = strchr(ms->words[1], '>');   // find the end of the email address
    if(end == NULL){
        if(send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    if(start !=NULL && !isValidPath(start)){
        if(send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    // check whether the email address after vrfy command is valid
    char *atExist = strchr(ms->words[1], '@');  // find a pointer pointing to '@'

    // check whether '@' exist and whether the domain server is valid
    if(atExist == NULL || !isValidDomain(atExist+1)) {
        if(send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    if ((state = checkstate(ms, HELOexist)) == 0) {
        if (send_formatted(ms->fd, "250 %s \r\n", "Requested mail action okay, completed") <= 0) return -1;
        ms->state = MAILexist;
        return 0;
    }
    return state;
}


int do_rcpt(smtp_state *ms) {
    dlog("Executing rcpt\n");
    // TODO: Implement this function
    char *emailAddr;

    if (ms->state != MAILexist && ms->state !=RCPTexist) {
        if (send_formatted(ms->fd, "503 %s\r\n", "Bad sequence of commands") <= 0) return -1;
        return 1;
    }
    if(ms->nwords != 2){
        if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    //Check whether the command start with "RCPT TO:<"
    if(strncasecmp(ms->words[1], "TO:<", 4)){
        if(send_formatted(ms->fd, "500 Syntax error, command unrecognized\r\n") <= 0) return -1;
        return 1;
    }
    char *start = strchr(ms->words[1], '<'); // find the start of the email address
    char *end = strchr(ms->words[1], '>');   // find the end of the email address

    //Check whether the command ends with '>' and make sure there are messages between '<' and '>'
    if(end == NULL || end-1 == start){
        if(send_formatted(ms->fd, "500 Syntax error, command unrecognized\r\n") <= 0) return -1;
        return 1;
    } else {
        emailAddr = trim_angle_brackets(start);
        user_list_add(&ms->userList,emailAddr);
    }

    // check whether the argument identifies a user
    if(is_valid_user(emailAddr, NULL) == 0){
        if(send_formatted(ms->fd,"550 %s - %s\r\n",  "No such user", emailAddr) <= 0) return -1;
        return 1;
    } else { //if the username exists
        if(send_formatted(ms->fd,"250 OK %s\r\n", "User Exists") <= 0) return -1;
        ms->state = RCPTexist;
        return 0;
    }
}     

int do_data(smtp_state *ms) {
    dlog("Executing data\n");
    // TODO: Implement this function
    int len;
    char template[] = "templateXXXXXX";

    if(ms->state != RCPTexist){
        if (send_formatted(ms->fd, "503 %s\r\n", "Bad sequence of commands") <= 0) return -1;
        return 1;
    }
    if(ms->nwords != 1) {
        if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }

    if(send_formatted(ms->fd,"354 Waiting for data, finish with <CR><LF>.<CR><LF> \r\n") <= 0) return -1;

    // Create the temporary file
    int temp_fd = mkstemp(template);

    if(temp_fd == -1){ // dealing with errors caused during creating the temporary file
        perror("mkstemp");
        if(send_formatted(ms->fd,"451 Requested action aborted: error in processing \r\n") <= 0) return -1;
        return 1;
    }

    while ((len = nb_read_line(ms->nb, ms->recvbuf)) >= 0) {
        if (ms->recvbuf[len - 1] != '\n') {
            // command line is too long, stop immediately
            send_formatted(ms->fd, "500 Syntax error, command unrecognized\r\n");
            break;
        }
        if (strlen(ms->recvbuf) < len) {
            // received null byte somewhere in the string, stop immediately.
            send_formatted(ms->fd, "500 Syntax error, command unrecognized\r\n");
            break;
        }

        // Remove CR, LF and other space characters from end of buffer
        while (isspace(ms->recvbuf[len - 1])) ms->recvbuf[--len] = 0;
        if(!strcasecmp(ms->recvbuf, ".")){
            break;
        }

        //check if the first character is a period and there are other characters on the line
        if(!strncasecmp(ms->recvbuf, ".", 1)){
            write(temp_fd, ms->recvbuf+1, strlen(ms->recvbuf)-1); // delete the first character
            write(temp_fd, "\r\n",2);
        } else{
            write(temp_fd, ms->recvbuf, strlen(ms->recvbuf));
            write(temp_fd, "\r\n",2);
        }
    }

    // Save the email messages for each recipient in userList of the message
    save_user_mail(template, ms->userList);
    user_list_destroy(ms->userList);
    ms->userList = user_list_create();

    // clear tmp file
    unlink(template);
    close(temp_fd);

    ms->state = HELOexist;
    if(send_formatted(ms->fd,"250 OK\r\n") <= 0) return -1;
    return 0;
}     
      
int do_noop(smtp_state *ms) {
    dlog("Executing noop\n");
    // TODO: Implement this function
    if(send_formatted(ms->fd,"250 OK (noop)\r\n")<= 0) return -1;
    return 0;
}

int do_vrfy(smtp_state *ms) {
    dlog("Executing vrfy\n");
    // TODO: Implement this function
    if(ms->nwords != 2){
        if (send_formatted(ms->fd, "501 %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
        return 1;
    }
    ms->words[1] = trim_angle_brackets(ms->words[1]);

    // check whether the argument identifies a user
    if(is_valid_user(ms->words[1], NULL) == 0){
        if(send_formatted(ms->fd, "550 %s - %s\r\n",  "No such user", ms->words[1]) <= 0) return -1;
        return 1;
    } else { //if the username exists
        if(send_formatted(ms->fd,"250 OK %s\r\n", "User Exists") <= 0) return -1;
        return 0;
    }
}

void handle_client(int fd) {
  
    size_t len;
    smtp_state mstate, *ms = &mstate;
  
    ms->fd = fd;
    ms->nb = nb_create(fd, MAX_LINE_LENGTH);
    ms->state = Undefined;
    uname(&ms->my_uname);

    ms->userList = user_list_create(); // initialize an empty userList
    
    if (send_formatted(fd, "220 %s Service ready\r\n", ms->my_uname.nodename) <= 0) return;

  
    while ((len = nb_read_line(ms->nb, ms->recvbuf)) >= 0) {
	if (ms->recvbuf[len - 1] != '\n') {
	    // command line is too long, stop immediately
	    send_formatted(fd, "500 Syntax error, command unrecognized\r\n");
	    break;
	}
	if (strlen(ms->recvbuf) < len) {
	    // received null byte somewhere in the string, stop immediately.
	    send_formatted(fd, "500 Syntax error, command unrecognized\r\n");
	    break;
	}
    
	// Remove CR, LF and other space characters from end of buffer
	while (isspace(ms->recvbuf[len - 1])) ms->recvbuf[--len] = 0;
    
	dlog("Command is %s\n", ms->recvbuf);
    
	// Split the command into its component "words"send_string
	ms->nwords = split(ms->recvbuf, ms->words);
	char *command = ms->words[0];
    
	if (!strcasecmp(command, "QUIT")) {
	    if (do_quit(ms) == -1) break;
	} else if (!strcasecmp(command, "HELO") || !strcasecmp(command, "EHLO")) {
	    if (do_helo(ms) == -1) break;
	} else if (!strcasecmp(command, "MAIL")) {
	    if (do_mail(ms) == -1) break;
	} else if (!strcasecmp(command, "RCPT")) {
	    if (do_rcpt(ms) == -1) break;
	} else if (!strcasecmp(command, "DATA")) {
	    if (do_data(ms) == -1) break;
	} else if (!strcasecmp(command, "RSET")) {
	    if (do_rset(ms) == -1) break;
	} else if (!strcasecmp(command, "NOOP")) {
	    if (do_noop(ms) == -1) break;
	} else if (!strcasecmp(command, "VRFY")) {
	    if (do_vrfy(ms) == -1) break;
	} else if (!strcasecmp(command, "EXPN") ||
		   !strcasecmp(command, "HELP")) {
	    dlog("Command not implemented \"%s\"\n", command);
	    if (send_formatted(fd, "502 Command not implemented\r\n") <= 0) break;
	} else {
	    // invalid command
	    dlog("Illegal command \"%s\"\n", command);
	    if (send_formatted(fd, "500 Syntax error, command unrecognized\r\n") <= 0) break;
	}
    }
  
    nb_destroy(ms->nb);
}
