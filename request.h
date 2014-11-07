#ifndef __REQUEST_H__

void requestHandle(int fd);

//Had to add these for server to call these functions in determining file size
void requestServeStatic(int fd, char *filename, int filesize);
void requestServeDynamic(int fd, char *filename, char *cgiargs);
void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


void requestReadhdrs(rio_t *rp);
int requestParseURI(char *uri, char *filename, char *cgiargs);

#endif
