#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>

#define MAXLEN 1024

// чтение строки со стандартного ввода
int read_string(char* str);
// выделение из строки str команды с уловным выполнением <Команда Shellа > → < Команда с условным выполнением >{ [; | &] < Команда Shellа>}{ ; |&}
int make_cond_command(char* str);
// <Команда с условным выполнением > → <Команда> { [&& | || ] <Команда с условным выполнением>}
int make_command(char *str);
// конвеер
int pipeline(char* str, int flag);
// выделение из конвеера команды
int get_simplecmd(char* str, int fd1, int fd2);
// выполнение команды
int execute_command(char** args, int fd1, int fd2);
// разбиение строки на массив
char** get_args(char* str);

void space_adder(char* str)
{
	char* tempstr = (char*)malloc(strlen(str)*2);
	if(tempstr == NULL)
	{
		fprintf(stderr,"Malloc error!");
		exit(1);
	}
	long i=0,j=0;
	while(str[i]!=0)
	{
		if(str[i]=='|' && str[i+1]!='|') // ls|wc -> ls | wc
		{
			tempstr[j++]=' '; tempstr[j++]='|'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='&' && str[i+1]!='&') // ls&wc -> ls & wc
		{
			tempstr[j++]=' '; tempstr[j++]='&'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]==';') // ls;wc -> ls ; wc
		{
			tempstr[j++]=' '; tempstr[j++]=';'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='>' && str[i+1]!='>') // ls>fout -> ls > fout
		{
			tempstr[j++]=' '; tempstr[j++]='>'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='<') // ls<fin -> ls < fin
		{
			tempstr[j++]=' '; tempstr[j++]='<'; tempstr[j++]=' ';
			++i;
		}
		else if(str[i]=='>' && str[i+1]=='>') // ls>>fout -> ls >> fout
		{
			tempstr[j++]=' '; tempstr[j++]='>'; tempstr[j++]='>'; tempstr[j++]=' ';
			i+=2;
		}
		else if(str[i]=='(' || str[i]==')')
		{
			tempstr[j++]=' ';
			++i;
		}
		else
			tempstr[j++]=str[i++];
	}
	tempstr[j]=0;
	strcpy(str,tempstr);
	free(tempstr);
}


void space_deleter(char* str)
{
	space_adder(str);
	char flag = (str[0]==' ');
	long i=0,j=0;
	while(str[i]!=0)
	{
		if(str[i]==' ')
		{
			if(flag)
			{
				++i;
				continue;
			}
			flag = 1;
		}
		else
			flag = 0;
		str[j++]=str[i++];	
	}
	str[j]=0;
	//printf("\nstring 2: %s\n",str);
}

int isflag(char *str, int *nn, int *nn1) // для tail
{
	int n;
	if (str[0]=='-') *nn=1;
	if (str[0]=='+') *nn1=1;
	str[0]='0';
	n=atoi(str);// конвертируем строку
	return n;
}

int main()
{
    for(;;)
    { 
        printf("$");
        char str[MAXLEN];
        read_string(str);
        space_deleter(str);
        if (!strcmp(str, "exit")) return 0;        
        make_cond_command(str);
    }  
    return 0;
}

int read_string(char* str)
{
    char c;
    int i=0;
    int spacecount=0;
    while((c=getchar()) != '\n')
    {
        str[i++]=c;
    }
    
    str[i]='\0';
    
    //printf("\n\nstr: %s\n\n",str);
    return 0;
}
 
int make_cond_command(char* str)
{
    int k1 = 0;
    if (str==NULL) return 1;
    size_t start = 0;
    size_t end = 0;
    int status;
    int ex_code = 0;
    
    while (str[end] != '\0' && str[end] != '\n')
    {
        while(str[end] != ';' && str[end] != '&' && str[end] != '\0' && str[end] != '\n')
        {
            end++;
        } 

        if(str[end] == ';')
        {
            str[end] = '\0';
            end++;
            ex_code = make_command(str+start);
            int n = MAXLEN;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        }
        else if(str[end] == '&') //условие на фоновый режим
        {
            pid_t p1, p2;
            p1 = fork();
            if (!p1) //сын запускает внука и умирает
            {
                p2 = fork();
                if(p2) // сын
                {
                    exit(0);
                }
                if(!p2) //внук работает в фоне
                {
                    signal(SIGINT, SIG_IGN);
                    int fd = open("/dev/null", O_RDWR);
                    dup2(fd, 0);
                    str[end] = '\0';
                    end++;                    
                    ex_code = make_command(str+start);
                    exit(ex_code);
                }
            }
            wait(NULL);
            end++;
            int n = MAXLEN;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        }
        else if(str[end] == '&' && str[end+1] == '&')
            end+=2;
        else if(str[end] == '\0' || str[end] == '\n')
        {
            ex_code = make_command(str+start);
            int n = MAXLEN;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
        }
    }
    return ex_code;    
}

int make_command(char *str)
{
    int k1 = 0;
    if (str == NULL) return 1;
    size_t start = 0;
    size_t end = 0;
    int flag = 1;
    int flag2 = 1;
    int ex_code = 0;
    while (str[end] != '\0' && str[end] != '\n')
    {
        while(str[end] != '&' && str[end] != '|' && str[end] != '\0' && str[end] != '\n')
        {
            end++;
        }
        if ((str[end] == '&' && str[end+1] == '&') || (str[end] == '|' && str[end+1] == '|')) //условие на запуск конвеера
        {
            if (str[end] == '&') flag2 = 1; else flag2 = 0; 
            str[end] = '\0';
            end+=2;
            if (flag == 1 && ex_code == 0 || flag == 0 && ex_code != 0) 
                ex_code = pipeline(str+start, flag);
            flag = flag2;
            
            int n = MAXLEN;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }

            start = end;
        }
        else if (str[end] != '\0' && str[end]!='\n')
            end++;
    }
    if (str[end] == '\0' || str[end] == '\n')
    {
        if (flag == 1 && ex_code == 0 || flag == 0 && ex_code != 0) 
            ex_code = pipeline(str+start, flag);  
    }
    return ex_code;
}

int pipeline(char* str, int flag)
{
    if (str == NULL) return 1;
    int fd1 = 0;
    int fd2 = 1;
    int j;
    int is_append = 0;
    int is_begin_append = 0;
    int is_begin_in = 0;
    int is_begin_out = 0;
    int f_in = 0;
    int f_out = 0;
    size_t start = 0;
    size_t end = 0;
    int n = MAXLEN;
    int ex_code = 0;
    
    while (n--) 
    {   
        if (str[end] == ' ') end++;
        else break;
    }

    while (str[end] != '\0' && str[end] != '\n')
    {
        if (str[end] == '<') f_in = 1;
        if (str[end] == '>') f_out = 1;
        end++;
    }
    end = 0;
    
    n = MAXLEN;
    while (n--)
    {   
        if (str[end] == ' ') end++;
        else break;
    }
    
    if (str[end] == '<') is_begin_in = 1;
    if (str[end] == '>') is_begin_out = 1;
    end = 0;
    if (f_in && !f_out && is_begin_in) //есть ввод, нет вывода, ввод в начале 
    {
        end = 0;
        while (str[end] != '<')
            end++;
        
        n = MAXLEN;
        end++;
        while (n--) 
        {   
            if (str[end] ==' ') end++;
            else break;
        }

        char f_in_name[MAXLEN];
        int j=0;
        while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
            f_in_name[j++]=str[end++];
        
        f_in_name[j]='\0';
        fd1 = open(f_in_name, O_RDONLY);
        printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
        end++;
        start = end;
        ex_code = get_simplecmd(str+start, fd1, fd2);
        return ex_code;
    }
    if (f_in && !f_out && !is_begin_in) // есть ввод, нет вывода, ввод в конце
    {
        end = 0;
        while (str[end] != '<')
            end++;
        n = MAXLEN;
        int init = end;
        end++;
        
        while (n--) 
        {   
            if (str[end] ==' ') end++;
            else break;
        }
        
        char f_in_name[MAXLEN];
        j = 0;
        while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
            f_in_name[j++]=str[end++];
        f_in_name[j]='\0';
        fd1 = open(f_in_name, O_RDONLY);
        printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name); 
        str[init]='\0';        
        ex_code = get_simplecmd(str+start, fd1, fd2);
        return ex_code;
    }
    if (f_out && !f_in && is_begin_out) //есть вывод, нет ввода, вывод в начале 
    {
        end = 0;
        while (str[end] != '>')
            end++;
        int append = (str[end+1] == '>');
        if (append) end++;
        n = MAXLEN;
        end++;
        while (n--) 
        {   
            if (str[end] ==' ') end++;
            else break;
        }
        char f_out_name[MAXLEN];
        int j = 0;
        while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
            f_out_name[j++]=str[end++];
        f_out_name[j]='\0';
        if (!append)
        {
            fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
        }
        else 
        {
            fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_APPEND, 0777);
        }
        printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
        end++;
        start = end;
        ex_code = get_simplecmd(str+start, fd1, fd2);
        return ex_code;
    }
    if (f_out && !f_in && !is_begin_out) // есть вывод, нет ввода, вывод в конце
    {
        end = 0;
        while (str[end] != '>')
            end++;
        int append = (str[end+1] == '>');
        int init = end;
        if (append) end++;
        n = MAXLEN;
        end++;
        while (n--) 
        {   
            if (str[end] ==' ') end++;
            else break;
        }
        char f_out_name[MAXLEN];
        j = 0;
        while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
            f_out_name[j++]=str[end++];
        f_out_name[j]='\0';
        if (!append)
        {
            fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
        }
        else 
        {
            fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_APPEND, 0777);
        }
        printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name); 
        str[init]='\0';        
        ex_code = get_simplecmd(str+start, fd1, fd2);
        return ex_code;
    }
    if (f_in && f_out && (is_begin_in || is_begin_out)) //есть и ввод и вывод, в начале
    {
        int first_in = 0;        
        int first_out = 0;   
        end = 0;
        while(str[end] != '>' && str[end] != '<')
            end++;
        if (str[end] == '>') 
            first_out = 1;
        else first_in = 1;
        if (first_out)     // сначала вывод
        {
            int append = (str[end+1] == '>');
            if (append) end++;
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            char f_out_name[MAXLEN];
            int j = 0;
            while(str[end] != ' ' && str[end] != '&' &&  str[end] != '|' && str[end] != '\0' && str[end] != '\n')
                f_out_name[j++]=str[end++];
            f_out_name[j] = '\0';
            if (!append)
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
            }
            else 
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            end++;
            char f_in_name[MAXLEN];
            j = 0;
            while(str[end] != ' ' && str[end] != '&' &&  str[end] != '|' && str[end] != '\0' && str[end] != '\n')
                f_in_name[j++] = str[end++];
            f_in_name[j]='\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            end++;
            start = end;
            ex_code = get_simplecmd(str+start, fd1, fd2);
            return ex_code;
        }
        if (first_in)     // сначала ввод
        {
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            char f_in_name[MAXLEN];
            int j = 0;
            while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
                f_in_name[j++]=str[end++];
            f_in_name[j]='\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            char f_out_name[MAXLEN];
            j = 0;
            int append = (str[end+1] == '>');
            if (append) end++;
            end++;
            while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
                f_out_name[j++]=str[end++];
            f_out_name[j]='\0';
            if (!append)
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
            }
            else 
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            end++;
            start = end;
            ex_code = get_simplecmd(str+start, fd1, fd2);
            return ex_code;
        }
    }
    if (f_in && f_out && (!is_begin_in && !is_begin_out)) //есть и ввод и вывод, в конце
    {
        int first_in = 0;        
        int first_out = 0;   
        end = 0;
        while(str[end] != '>' && str[end] != '<')
            end++;
        if (str[end] == '>') 
            first_out = 1;
        else first_in = 1;
        int init = end;
        if (first_out)     // сначала вывод
        {
            int append = (str[end+1] == '>');
            if (append) end++;
 
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] ==' ') end++;
                else break;
            }
            char f_out_name[MAXLEN];
            int j = 0;
            while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
                f_out_name[j++]=str[end++];
            f_out_name[j]='\0';
            if (!append)
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
            }
            else 
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            n = MAXLEN;
            end++;
            end++;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            char f_in_name[MAXLEN];
            j = 0;
            while(str[end]!= ' ' && str[end] != '&' &&  str[end] != '|' && str[end] != '\0' && str[end] != '\n')
                f_in_name[j++]=  str[end++];
            f_in_name[j] = '\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            end++;
            str[init] = '\0';
            ex_code = get_simplecmd(str+start, fd1, fd2);
            return ex_code;
        }
        if (first_in)     // сначала ввод
        {
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] ==' ') end++;
                else break;
            }
            char f_in_name[MAXLEN];
            int j = 0;
            while(str[end]!=' ' && str[end]!='&' &&  str[end]!='|' && str[end]!='\0' && str[end]!='\n')
                f_in_name[j++]=str[end++];
            f_in_name[j]='\0';
            fd1 = open(f_in_name, O_RDONLY);
            printf("Ввод был перенаправлен на ввод из файла %s\n", f_in_name);
            n = MAXLEN;
            end++;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            int append = (str[end+1] == '>');
            if (append) end++;
            end++;
            char f_out_name[MAXLEN];
            j = 0;
            while(str[end] != ' ' && str[end] != '&' &&  str[end] != '|' && str[end] != '\0' && str[end] != '\n')
                f_out_name[j++]=str[end++];
            f_out_name[j]='\0';
            if (!append)
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_TRUNC, 0777);
            }
            else 
            {
                fd2 = open(f_out_name, O_WRONLY|O_CREAT|O_APPEND, 0777);
            }
            printf("Вывод был перенаправлен на вывод в файл %s\n", f_out_name);
            end++;
            str[init] = '\0';
            ex_code = get_simplecmd(str+start, fd1, fd2);
            return ex_code;
        }   
    }
    ex_code = get_simplecmd(str, fd1, fd2);  //перенаправления нет вообще
    return ex_code;
}

int get_simplecmd(char* str, int fd1, int fd2)
{

    int sons_counter = 1;
    int status = 0;
    int temp = 0;
    pid_t p;
    if (str == NULL)
        return 1;
    int marker = 0;
    char string[MAXLEN];
    strcpy(string, str);
    size_t start = 0;
    size_t end = 0;
    int fl = 0;
    
    while (string[end] != '\0' && string[end] != '\n')
    {
        if (string[end] == '|')
        sons_counter++;
        
        end++;
    }
    end = 0;
    if(sons_counter == 1)
    {
        while(string[end] != '\0' && string[end] != '\n')
            end++;
        string[end] = '\0';
        char** args = get_args(string + start);
        p = fork();
        if (!p)
            execute_command(args, fd1, fd2);
        wait(&status);
        return status;
    }
        
    int fdd0 = fd1; //ввод в текущую команду
    int fdd1; // вывод из текущей команды
    int fd[2];
    for (int i = 1; i < sons_counter;)
    {
        end++;
        if (string[end] == '|')
        {
            string[end] = '\0';
            char** args = get_args(string + start);
            end++;
            
            int n = MAXLEN;
            while (n--) 
            {   
                if (str[end] == ' ') end++;
                else break;
            }
            start = end;
            
            pipe(fd); // канал между текущей и следующей командой
            fdd1 = fd[1]; //вывод из текущей направляем в канал
            p = fork();
            if (!p) 
                execute_command(args, fdd0, fdd1);
            if (fdd0 != 0)  //закрываем на чтение канал у текущей команды 
                close(fdd0);
            close(fdd1); 
            fdd0 = fd[0]; //ввод следующей перенаправляем на канал
            i++;
        }
    }
    while(string[end] != '\0' && string[end] != '\n')
        end++;
    string[end] = '\0';
    char** args = get_args(string + start);
    fdd1 = fd2; //вывод последней перенаправляем
    p = fork();
    if (!p)
        execute_command(args, fdd0, fdd1);
    close(fdd0); 
    if (fdd1 != 1)
        close(fdd1);
    
    for (int i = 0; i<sons_counter; i++) // ждем всех сыновей
    {
        wait(&temp);
        if (status == 0)
            status = temp;
    }
    return status;
}

int execute_command(char** args, int fd1, int fd2)
{
    char str1[MAXLEN];
    

    if (fd1 != 0) dup2(fd1, 0);
    if (fd2 != 1) dup2(fd2, 1);


		if (strcmp(args[0], "exit") == 0)
        { //exitsa
			return -1;
		}
		else if(strcmp(args[0], "cd") == 0)
		{
    			if (args[1] == NULL)
    			{
    				chdir("..");
    			}
    			else
    			{
    				chdir(args[1]);
    				
    			}
    			return 0;
		}
		else if(strcmp(args[0], "mv") == 0) 
		{
			if (args[1] == NULL)
				fprintf(stderr, "shell: waiting for 'mv' args\n");
			struct stat stbuf;
			int i = 0;
			int argc = 0;
			while(args[i] != NULL)
			{
				argc++;
				i++;
			}
			stat(args[argc - 1], &stbuf); // заполняем структуру stbuf информацией из файла
			if ((stbuf.st_mode & S_IFMT) == S_IFDIR) // st_mode - режим доступа, смотрим каталог ли у нас или файл
				{
					char *ndir=NULL;
					for (int i=1; i < (argc-1); ++i)
					{
						int j=0;
						while (args[argc-1][j] != '\0')
						{
							ndir = (char*) realloc(ndir, (j+1) * sizeof(char));
							++j;
						}
						ndir = (char*) realloc(ndir, (j+1) * sizeof(char));
						ndir[j] = '\0';
						strcpy(ndir, args[argc-1]);
						strcat(ndir,args[i]);//
						link(args[i], ndir);//
						unlink(args[i]);//
						free(ndir);
						ndir=NULL;
					}
				}
			else if (argc==3)
				rename(args[1], args[2]);
			else
				printf("ERROR");
			return 0;
		}
		else if(strcmp(args[0], "grep") == 0) // >grep substring file
		{
			FILE *f;
			char *str = NULL;
			int i=0;
			char c;
			if (args[1] == NULL)
				exit(1);
			if (!(strcmp(args[1], "-v\0")))
			{
				if ((f = fopen(args[3],"r")) == NULL)
					exit(1);
				while ((c = fgetc(f)) != EOF)
				{
					if (c != '\n')
					{
						if (str == NULL)
							str = (char*) malloc(sizeof(char));
						str[i] = c;
						++i;
						str = (char*) realloc(str, (i+1) * sizeof(char));
					}
					else
					{
						str[i] = '\0';
						if (strstr(str, args[2]) == NULL)
						{
							fputs(str, stdout);
							printf("\n");
						}
						free(str);
						str = NULL;
						i = 0;
					}
				}
			}
			else
			{
				if ((f = fopen(args[2],"r")) == NULL)
					exit(1);
				while ((c = fgetc(f)) != EOF)
				{
					if (c != '\n')
					{
						if (str == NULL)
							str = (char*) malloc(sizeof(char));
						str[i] = c;
						++i;
						str = (char*) realloc(str, (i+1) * sizeof(char));
					}
					else
					{
						str[i] = '\0';
						if (strstr(str, args[1]) != NULL)
						{
							fputs(str, stdout);
							printf("\n");
						}
						free(str);
						str = NULL;
						i = 0;
					}
				}
			}
			fclose(f);
			return 0;
		}
		else if(strcmp(args[0], "tail") == 0)
		{
			char buf[1024];
			int fd1,n;
			int countstrings=0;
			int symbols=0;
			int posn=0, k, p, numm=1;
			int numst=10;
			int nn=0, nn1=0;
			int numstring;
			int numargs=1;
			int i = 0;
			int argc = 0;
			while(args[i] != NULL)
			{
				argc++;
				i++;
			}
			if (argc>2)
			{		
				numst=isflag(args[1], &nn, &nn1);
				numargs=2;
			}
		
			if((fd1=open(args[numargs],O_RDONLY)) == -1)
			{
				printf("Error! Cant open file\n");
				return -1;
			}
			if (!nn1)
			{
				while((n=read(fd1,buf,sizeof(buf)))>0)
				{
					int i=0;
					while(buf[i])
					{
						if (buf[i]=='\n')
						{
							++countstrings;
						}
						++i;
					}
				}
				numstring=countstrings-numst;
			}
			else
				numstring=numst;
			int f=0;
			int c=0;
			if (numstring<0) numstring=0;
			else
			{
				p=lseek(fd1, 0, SEEK_SET);
				while((n=read(fd1,buf,sizeof(buf)))>0)
				{
					int i=0;
					while(buf[i])
					{
						++symbols;
						if (buf[i]=='\n')
						{
							++c;
							posn=symbols;
							if (c==numstring)
							{
								f=1;
								break;
							}
						}
						++i;
					}
					if (f==1) break;
				}
			}
			close(fd1);
			FILE *f1;
			f1=fopen(args[numargs], "r+");
			fseek(f1, sizeof(buf[posn])*posn, SEEK_SET);
			while((fgets(buf, 1024, f1))!=NULL)
				printf("%s", buf);
			fclose(f1);
		}
		else
		{
			execvp(args[0],args);
			exit(1);
		}
}

char** get_args(char* str) // разделяем выражение на аргументы
{
    int start = 0;
    int end = 0;
    int args_counter = 1;
    while (str[end] != '\n' && str[end] != '\0')
    {
        if (str[end] == ' ' && str[end+1] != ' ' && str[end+1] != '\0')
            args_counter++;
        end++;
    }
    end = 0;
    char **args = (char **)malloc(args_counter*sizeof(char *));
    for(int i = 0; i < args_counter; i++) 
    {
        args[i] = (char *)malloc(MAXLEN*sizeof(char));
    }
    int i = 0;
    while (str[end] != '\n' && str[end] != '\0')
    {
        while (str[end] != ' ' && str[end] != '\n' && str[end] != '\0')
            end++;
        if (str[end] == '\0')
        {
            strcpy(args[i++], str+start);
            break;
        }
        str[end] = '\0';
        end++; 
        strcpy(args[i++], str+start);
        start = end;
    }
   
    //for (i = 0; i<args_counter; i++)
     //   printf("-------------------------- %s\n", args[i]);
    
    args[i] = NULL;
    
    return args;
}
