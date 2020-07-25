/***************Dev_Branch************/
/***************For furthur Modification******/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<termios.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<ctype.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<string.h>
#include<time.h>
#include<stdarg.h>


#define CTRL_KEY(k) (k& 0x1f)
#define ABUF_INIT {NULL,0}
#define TEXT_EDITOR_STRING "TEXT EDITOR --VERSION"
#define TEXT_EDITOR_VERSION "0.0.1"
#define TAB_SIZE 8
#define QUIT_TIMES 3
void Editor_Set_Status_Message(const char *fmt,...);
void Editor_Refresh_Screen();
void Editor_Save();
char * Editor_Prompt(char * prompt,void (*callback)(char *,int));
typedef struct erow{
int size;
char *ptr;
int rsize;
char *render;
}EROW;
struct Editor_Config{
	// state of Editor
	int rx;
	int cx;
	int cy;
	int ScreenRow;
	int rowoff;
	int coloff;
	int ScreenCol;
    EROW *row;
    int numrows;
    char *filename;
    char status_msg[80];
    time_t statusmsg_time;
	struct termios orig_termios;
	int dirty;
};
struct Abuf{
	char *s;
	int len;

};
enum Editor_Key{
	BACKSPACE=127,
	ARROW_LEFT=1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	PAGE_UP,
	PAGE_DOWN,
	HOME_KEY,
	END_KEY
};
struct Editor_Config E;



/*********************print_error******************/

void print_error(char *s)
{
  write(STDOUT_FILENO,"\x1b[2J",4);
  write(STDOUT_FILENO,"\x1b[H",3);
  printf("%s\n",s);
  exit(1);
}

/*********************Editor_Set_Status_Message*****/

void Editor_Set_Status_Message(const char *fmt,...){
	va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);


}

/*********************Editor_Rowcxtorx*************/
int Editor_Rowcxtorx(EROW *row,int cx){
	int rx=0;
	for(int i=0;i<cx;i++)
	{
		if(row->ptr[i]=='\t')
		rx+=(TAB_SIZE -1 ) -(rx%TAB_SIZE);
	    rx++;
	}
    return rx;
}
/*********************Editor_Update*********************/

void Editor_UpdateRow(EROW *row){
int tabs=0;
for(int i=0;i<row->size;i++)
{
	if(row->ptr[i]=='\t')
		tabs++;
}
free(row->render);
row->render=malloc(row->size+tabs*(TAB_SIZE-1)+1);
// Now it's time to Copy chars of that Row
int idx=0;
for(int i=0;i<row->size;i++)
{
	if(row->ptr[i]=='\t')
	{
		row->render[idx++]=' ';
		while(idx%8!=0)
		row->render[idx++]=' ';
	}
	else
	row->render[idx++]=row->ptr[i];
}
row->render[idx]='\0';
row->rsize=idx;

}

/*********************Editor_Row_Insert_Char*************/

void Editor_Row_Insert_Char(EROW *row,int at,char c){

if(at <0 || at >0 )at=row->size; // Default Case In the end I'll insert char
row->ptr=realloc(row->ptr,row->size+2); // row_size doesn't count NULL bytes
// But for NULL char we also have to allocate memory 
// How much One byte
// We can't use Memcpy fun
// That's why we have to use memmove()
memmove(&row->ptr[at+1],&row->ptr[at],/*How many bytes*/ row->size-at+1);// Difference +1
row->size++;// Update Row size
// Insert char
row->ptr[at]=c;
// Update render and rsize
Editor_UpdateRow(row);
E.dirty++;
}
/*********************Editor_Free_Row********************/
void Editor_Free_Row(EROW *row){
	free(row->ptr);
	free(row->render);
}
/*********************Editor_Row_Del*********************/

void Editor_Row_Del(int at){
if(at<0 || at>=E.numrows)return;
Editor_Free_Row(&E.row[at]);
memmove(&E.row[at],&E.row[at+1],sizeof(EROW)*(E.numrows-1-at));
E.numrows--;
E.dirty++;
}

/*********************Editor_Row_Append_String************/

void Editor_Row_Append_String(EROW *row,char *s,int len){
	row->ptr=realloc(row->ptr,row->size+len+1);
	memcpy(&row->ptr[row->size],s,len);
	row->size+=len;
	row->ptr[row->size]='\0';
	Editor_UpdateRow(row);
	E.dirty++;
}

/*********************Editor_Row_Del_Char****************/

void Editor_Row_Del_Char(EROW *row,int at){
if(at<0 || at>row->size)return ;
memmove(&row->ptr[at],&row->ptr[at+1],row->size-(at+1)+1);
row->size--;
Editor_UpdateRow(row);
E.dirty++;


}

/*********************Editor_Del_Char********************/

void Editor_Del_Char(){
	if(E.cx==0 && E.cy==0)
		return ;
	if(E.cy==E.numrows)
		return ;
	EROW *row=&E.row[E.cy];
	if(E.cx>0)
	{
		Editor_Row_Del_Char(row,E.cx-1);
		E.cx--;
	}
	else
	{
		// Which Row
		E.cx=E.row[E.cy-1].size;
		Editor_Row_Append_String(&E.row[E.cy-1],row->ptr,row->size);
		Editor_Row_Del(E.cy);
		E.cy--;
	}

}

/*********************Editor_Append_Row******************/

void Editor_Insert_Row(int at,char *s,size_t len){
	if(at <0 || at>E.numrows)return;
E.row=(EROW *)realloc(E.row,sizeof(EROW)*(E.numrows+1));
memmove(&E.row[at+1],&E.row[at],sizeof(EROW)*(E.numrows-at));
E.row[at].size=len;// Ask Question Yourself Which row
E.row[at].ptr=(char *)malloc(len+1);
memcpy(E.row[at].ptr,s,len);
E.row[at].ptr[len]='\0';
E.row[at].rsize=0;
E.row[at].render=NULL;
Editor_UpdateRow(&E.row[at]);// which Row I want to update
E.numrows++;// It gets incremented
E.dirty++;
}

/********************Editor_Newline**********************/

void Editor_Insert_NewLine(){
if(E.cx==0)// Add newLine
	Editor_Insert_Row(E.cy," ",1);
else
{
	EROW *row=&E.row[E.cy];
	Editor_Insert_Row(E.cy+1,&row->ptr[E.cx],row->size-E.cx);
	row=&E.row[E.cy];
	row->size=E.cx;
	row->ptr[row->size]='\0';
	Editor_UpdateRow(row);
}
E.cy++;
E.cx=0;



}


/********************Editor_Insert_Char******************/

void Editor_Insert_Char(int c){
if(E.cy==E.numrows)
{
	Editor_Insert_Row(E.numrows," ",0);

}
Editor_Row_Insert_Char(&E.row[E.cy],E.cx,c);
E.cx++;// Just put the cursor after new inserted char


}

/*******************Editor_Rows_to_String***************/

char * Editor_Rows_To_String(int *buflen){
int buf_size=0;
for(int i=0;i<E.numrows;i++)
{
	buf_size+=E.row[i].size+1;
}
*buflen=buf_size;
char * buf=malloc(buf_size);
char *p=buf;// Temporary pointer
for(int i=0;i<E.numrows;i++)
{
	memcpy(p,E.row[i].ptr,E.row[i].size);
	p+=E.row[i].size;
	*p='\n';
	p++;// For next Row
}
return buf;
}

/******************Editor_Row_Rx_to_cx****************/

int Editor_Row_Rx_To_Cx(EROW *row,int rx){
int cur_rx=0;
int cx=0;
for( cx=0;cx<row->size;cx++)
{
	if(row->ptr[cx]=='\t')
		cur_rx+=(TAB_SIZE-1)-(cur_rx%TAB_SIZE);
	    cur_rx++;
	if(cur_rx>rx)
		return cx;
}
return cx;
}
/******************Editor_Find_Call_Back**************/

void Editor_Find_Call_Back(char *query,int key){
	if(key=='\r' || key=='\x1b')
		return ;
    for(int i=0;i<E.numrows;i++)
	{
		EROW *row=&E.row[i];
		char *match=strstr(row->render,query);
		if(match){
			E.cx=Editor_Row_Rx_To_Cx(row,(int)(match-row->render));
			E.cy=i;
			E.rowoff=E.numrows;
			break;
		}
	}
	
}



/******************Editor_Find************************/

void Editor_Find(){
	//Store current position
	int saved_cx=E.cx;
	int saved_cy=E.cy;
	int saved_rowoff=E.rowoff;
	int saved_coloff=E.coloff;
	char *query=Editor_Prompt("Search:%s (ESC to cancel)",Editor_Find_Call_Back);
	if(query){
		free(query);
	}
	else
	{
		// I want to Go to my past Position
		E.cx=saved_cx;
		E.cy=saved_cy;
		E.rowoff=saved_rowoff;
		E.coloff=saved_coloff;
	}
	
	
}

/******************Editor_Save************************/

void Editor_Save(){
	if(E.filename==NULL)
    {// We don't know about it now
      E.filename=Editor_Prompt("Save as: %s",NULL);
      if(E.filename==NULL)
      {
      	Editor_Set_Status_Message("Save Aborted");
      	return ;
      }
    }
	int len;
	char *buf=Editor_Rows_To_String(&len);
	int fd=open(E.filename,O_RDWR | O_CREAT,0644);
    if(fd!=-1)
    {
    	if(ftruncate(fd,len)!=-1)
    	{
    		if(write(fd,buf,len)==len)
    		{
    			close(fd);
    		    free(buf);
    		    E.dirty=0;
    		    Editor_Set_Status_Message("%d Number of bytes written to disk",len);
    		    return;
    		}
    	}

    }
	free(buf);
	Editor_Set_Status_Message("Can't Save File I/O error:%s",strerror(errno));
}
/*********************Editor_Open******************/
void Editor_Open(char *filename){
	free(E.filename);
	E.filename=strdup(filename);
FILE *fp=fopen(filename,"r");
if(!fp)print_error("Editor_Open");
char *Line_buff=NULL;
size_t Line_buff_size=0;
ssize_t Line_size;
while((Line_size=getline(&Line_buff,&Line_buff_size,fp))!=-1){
	while(Line_size>0 && ( Line_buff[Line_size-1]==
		'\n' ||  Line_buff[Line_size-1]=='\r'))
		Line_size--;
	Editor_Insert_Row(E.numrows,Line_buff,Line_size);
}
free(Line_buff);
fclose(fp);
E.dirty=0;
}



/*********************Abuf_Append******************/
void Abuf_Append(struct Abuf *af,char *c,int len){
	char *ptr=(char *)realloc(af->s,af->len+len);
	if(ptr==NULL)
		return;
	memcpy(&ptr[af->len],c,len);
	af->s=ptr;
	af->len+=len;
}



/*********************Abuf_Free********************/

void Abuf_Free(struct Abuf *af){
	free(af->s);
}







/*********************Get_Window_Size******************/

int  Get_Window_Size(int *row,int *col){
	struct winsize ws;
	if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==-1 || ws.ws_col==0)
		return -1;
	else
	{
		*row=ws.ws_row;
		*col=ws.ws_col;
		return 0;
	}
}



/*********************Inti_Editor******************/

void Inti_Editor(){
	E.rx=0;
	E.cx=0;
	E.cy=0;
	E.numrows=0;
	E.rowoff=0;
	E.coloff=0;
	E.dirty=0;
	E.row=NULL;
	E.filename=NULL;
	E.status_msg[0]='\0';
	E.statusmsg_time=0;
	if(Get_Window_Size(&E.ScreenRow,&E.ScreenCol)==-1)
	print_error("error:Inti_Editor");
    E.ScreenRow-=2;
}




/*********************disable_raw_mode******************/

void disable_raw_mode(){
if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.orig_termios)==-1)print_error("error:tcgetattr");
}




/*********************enable_raw_mode******************/

void enable_raw_mode(){
if(tcgetattr(STDIN_FILENO,&E.orig_termios)==-1)print_error("error:tcgetattr");// get those attributes
// Don't play with user be honest with user
// When program exits
// Restore original attributes we can't play with user
atexit(disable_raw_mode);// Everything is working fine

struct termios raw=E.orig_termios;
/* Now what we want
 turn off input echoing
 use struct raw call its member c_lflag
 We are in canonical mode
Turn off canonical mode using ICANON (bit flag)
Turn off Ctrl-c Ctrl-x Ctrl-y ctrl-\
*/

// Turn off OPOST We don't want translation \n=\r\n
raw.c_oflag&=~OPOST;


raw.c_iflag&=~ICRNL;
raw.c_iflag&=~(IXON);
raw.c_lflag&=~(ECHO | ICANON | ISIG);//Now we modified struct termios
// we have to set these attributes to terminal
raw.c_iflag&=~(BRKINT | INPCK | ISTRIP);
raw.c_cflag|=CS8;
 raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] =1;
if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==-1)print_error("error:tcsetattr");;
// It's done
}

/*********************editor_read_key******************/

int editor_read_key(){
	int nread;
	char c;
	while((nread=read(STDIN_FILENO,&c,1)!=1))
	{
		// Function waits indefinitely
		if(nread==-1 && errno!=EAGAIN)print_error("error:read");
	}
	if(c=='\x1b')
	{
		char seq[3];
		if(read(STDIN_FILENO,&seq[0],1)!=1)return '\x1b';
		if(read(STDIN_FILENO,&seq[1],1)!=1)return '\x1b';
		if(seq[0]=='['){
			if(seq[1]>='0' && seq[1]<='9'){
            if(read(STDIN_FILENO,&seq[2],1)!=1)return '\x1b';
            if(seq[2]=='~')
            {
            	switch(seq[1])
            	{
            		case '5':
            	return PAGE_UP;
            	case '6':
            	return PAGE_DOWN;
            	case '4':
            	return END_KEY;
            	case '8':
            	return END_KEY;
            	case '1':
            	return HOME_KEY;
            	case '7':
            	return HOME_KEY;
            	case '3':
            	return DEL_KEY;
            	}
            }
			}
			else
			{
			  switch(seq[1])
			  {
				case 'A':return ARROW_UP;
				case 'B':return ARROW_DOWN;
				case 'C':return ARROW_RIGHT;
                case 'D':return ARROW_LEFT;
                case 'H':return HOME_KEY;
                case 'F':return END_KEY;
			  }
		    }
		}
		else if(seq[0]=='O')
		{
			switch(seq[1]){
				case 'H':
				return HOME_KEY;
				case 'F':
				return END_KEY;
			}
		}
		return '\x1b';
	}
	else
	return c;

}

/*********************Editor_Draw_Rows******************/

void Editor_Draw_Rows(struct Abuf *af){
	
	for(int i=0;i<E.ScreenRow;i++)
	{
		int filerow=i+E.rowoff;
		if(filerow>=E.numrows){// After certain no of rows
		if(E.numrows==0  && i==E.ScreenRow/3)// No lines
		{
         char welcome[80];
         int nchars=snprintf(welcome,sizeof(welcome),"%s %s",TEXT_EDITOR_STRING,TEXT_EDITOR_VERSION);
         
         if(nchars>=E.ScreenCol)nchars=E.ScreenCol; // in case  terminal size is tiny
         int padding=(E.ScreenCol - nchars)/2;
         if(padding)
         {
         	Abuf_Append(af,"~",1);
         	padding--;
         }
         while(padding--)
         	Abuf_Append(af," ",1);
         Abuf_Append(af,welcome,nchars);
		}

		else
		{
        Abuf_Append(af,"~",1);
        }
    }
    else
    {
     int len=E.row[filerow].rsize-E.coloff;
     if(len<0)len=0;
     if(len>E.ScreenCol)len=E.ScreenCol;
     Abuf_Append(af,&E.row[filerow].render[E.coloff],len);
    }

		Abuf_Append(af,"\x1b[K",3);
		
			Abuf_Append(af,"\r\n",2);
	}
}

void Editor_Move_Cursor(int c){
	EROW *row=(E.cy >= E.numrows)?NULL:&E.row[E.cy];
	switch(c){
		case ARROW_UP:
		if(E.cy>0)
		{
			E.cy--;
			
		}
		break;
		case ARROW_LEFT:
		{
			if(E.cx!=0)
				E.cx--;
			else if(E.cy>0)
			{
				E.cy--;
				E.cx=E.row[E.cy].size;
			}
		}
		break;
		case ARROW_DOWN:
		
			if(E.cy<E.numrows)
			{
              E.cy++;
              
			}
		
		break;
		case ARROW_RIGHT:
		{
			if(row && E.cx <row->size)// Important condtion only up to how many chars are there
			E.cx++;
		    else if(row && E.cx==row->size)// Important feature
		    {
		    	// Move to next Line
		    	E.cy++;
		    	E.cx=0;
		    }
		}
		break;
        

	}
	row=(E.cy>=E.numrows)?NULL:&E.row[E.cy];
	int rowlen=(row)?row->size:0;
	if(E.cx >rowlen)
	E.cx=rowlen;

}


/*********************Editor_Key_Process******************/

void Editor_Key_Process(){
	static int quit_times=QUIT_TIMES;
	int c=editor_read_key();
	switch(c){
		case '\r':
		Editor_Insert_NewLine();
		break;

		case CTRL_KEY('q'):
		{
        if(E.dirty && quit_times>0)
        {
        	Editor_Set_Status_Message("WARNING!!File has unsaved changes. Press CTRL -Q %d more times to quit ",quit_times);
        	quit_times--;
        	
        	return;
        }
		write(STDOUT_FILENO,"\x1b[2J",4);
	    write(STDOUT_FILENO,"\x1b[H",3);
		exit(0);
		break;
        }
        case CTRL_KEY('s'):
		Editor_Save();
		break;

		case HOME_KEY:
		E.cx=0;
		break;
		case END_KEY:
		{
			if(E.cy <E.numrows)
				E.cx=E.row[E.cy].size;
		}
		break;
		case CTRL_KEY('f'):
		Editor_Find();
		break;
		case BACKSPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
		{
			if(c==DEL_KEY)
				Editor_Move_Cursor(ARROW_RIGHT);
			Editor_Del_Char();
		}
		break;
		case CTRL_KEY('l'):
		case '\x1b':
		/* We want to do something*/
		break;
		
		case PAGE_DOWN:
		case PAGE_UP:
		{
			if(c==PAGE_UP)
			E.cy=E.rowoff;
		    else if(c==PAGE_DOWN)
		    	E.cy=E.rowoff+E.ScreenRow-1;
		    if(E.cy>E.numrows)E.cy=E.numrows;
		    int times=E.ScreenRow;
		    while(times--)
		    	Editor_Move_Cursor(c==PAGE_UP?ARROW_UP:ARROW_DOWN);
		}
		break;
		case ARROW_UP:
		case ARROW_LEFT:
		case ARROW_DOWN:
		case ARROW_RIGHT:
		Editor_Move_Cursor(c);
		break;
		default:
		Editor_Insert_Char(c);
		break;

	}
	quit_times=QUIT_TIMES;
}


void Editor_Scroll(){
	//Here I am 
	E.rx=0;
	if(E.cy<E.numrows)
	    E.rx=Editor_Rowcxtorx(&E.row[E.cy],E.cx);
	if (E.cy < E.rowoff) 
    E.rowoff = E.cy;
  
	if(E.cy >=E.ScreenRow+E.rowoff)
		E.rowoff=E.cy -E.ScreenRow+1;
	if(E.rx<E.coloff)
		E.coloff=E.rx;
	if(E.rx >=E.ScreenCol+E.coloff)
		E.coloff=E.rx -E.ScreenCol+1;
}

/*********************Editor_Draw_Message_Bar****************/
void Editor_Draw_Message_Bar(struct Abuf *af){
Abuf_Append(af,"\x1b[K",3);
int msg_len=strlen(E.status_msg);
if(msg_len>E.ScreenCol)msg_len=E.ScreenCol;
if(msg_len && time(NULL)-E.statusmsg_time<5)
	Abuf_Append(af,E.status_msg,msg_len);
}


/*********************Editor_Draw_Status_Bar*****************/
void Editor_Draw_Status_Bar(struct Abuf *af){

Abuf_Append(af,"\x1b[7m",4);
// Make a your own buffer
char t[80];
char u[80];
int Len=snprintf(t,sizeof(t),"%.20s - %d No. Of Lines %s",(E.filename?E.filename:"[No Name]"),E.numrows,E.dirty?"Modified":"");
int Len2=snprintf(u,sizeof(u),"%d/%d",E.cy+1,E.numrows);
if(Len>E.ScreenCol)Len=E.ScreenCol;
Abuf_Append(af,t,Len);
// Status Of File Means FILENAME ,NO. OF LINES IN FILE
for(int i=Len;i<E.ScreenCol-5;i++)
Abuf_Append(af," ",1);
Abuf_Append(af,u,Len2);
Abuf_Append(af,"\x1b[m",3);
Abuf_Append(af,"\r\n",2);

}

/*********************Editor_Refresh_Screen******************/

void Editor_Refresh_Screen(){
	Editor_Scroll();
	struct Abuf abuf=ABUF_INIT;// Declare Abuf and intialize it
	Abuf_Append(&abuf,"\x1b[?25l",6);
	
	Abuf_Append(&abuf,"\x1b[H",3);// pass Repostion cursor string
	Editor_Draw_Rows(&abuf);
	Editor_Draw_Status_Bar(&abuf);// pass abuf and call it by reference
    //Abuf_Append(&abuf,"\x1b[H",3);
    Editor_Draw_Message_Bar(&abuf);
    char buf[32];
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",(E.cy-E.rowoff) +1 ,(E.rx -E.coloff)+ 1);
    Abuf_Append(&abuf,buf,strlen(buf));
    Abuf_Append(&abuf,"\x1b[?25h",6);
    // Now we will use write() 
    write(STDOUT_FILENO,abuf.s,abuf.len);
    Abuf_Free(&abuf);
}
/********************Editor_Prompt*******************/

char * Editor_Prompt(char *prompt,void(*callback)(char *,int)){
size_t buf_size=128;
char *buf=malloc(buf_size);
size_t buf_len=0;
buf[0]='\0';
while(1){
	Editor_Set_Status_Message(prompt,buf);
	Editor_Refresh_Screen();
	int c=editor_read_key();
	if(c=='\x1b')
	{
		Editor_Set_Status_Message("");
		if(callback)callback(buf,c);
		free(buf);
		return NULL;
	}
	else if(c=='\r')
	{
		if(buf_len!=0)
		{
			Editor_Set_Status_Message("");
			if(callback)callback(buf,c);
			return buf;
		}

	}
	else if(!iscntrl(c) && c<128)
	{
      if(buf_len==buf_size-1)
      {
      	buf_size*=2;
      	buf=realloc(buf,buf_size);
      }
      buf[buf_len++]=c;
      buf[buf_len]='\0';
	}
	if(callback)callback(buf,c);
}


}
int main(int argc,char *argv[]){
enable_raw_mode();
Inti_Editor();
if(argc>=2)
{
	Editor_Open(argv[1]);
}
Editor_Set_Status_Message("HELP: CTRL -S =SAVE | CTRL -Q =QUIT | CTRL -F=find");
while(1){
	Editor_Refresh_Screen();
	Editor_Key_Process();
}







}
