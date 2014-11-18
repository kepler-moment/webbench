/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 * 
 */ 
#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

/* values */
volatile int timerexpired=0;	//压力测试时间是否过期
int speed=0;	//连接成功的次数
int failed=0;	//连接失败的次数
int bytes=0;	//接收到的字节数

/* globals */
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define METHOD_POST 4
#define PROGRAM_VERSION "1.5"
int method=METHOD_GET;	//默认为GET

//default value
int clients=1;		//并发数
int force=0;		//是否等待服务器响应
int force_reload=0;	//是否缓存页面
int proxyport=80;	//代理服务器端口
char *proxyhost=NULL;	//代理服务器ip地址
int benchtime=30;	//默认测试时间30s

/* internal */
int mypipe[2];	//写数据的管道
char host[MAXHOSTNAMELEN];	//主机名
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];	//请求报文
#define POSTDATA_SIZE 512	//定义post数据最大字节数
char postData[POSTDATA_SIZE];
char lengthOfdata[POSTDATA_SIZE];	//保存postData长度

//可传递的参数设置
static const struct option long_options[]=
{
 {"force",no_argument,&force,1},
 {"reload",no_argument,&force_reload,1},
 {"time",required_argument,NULL,'t'},
 {"help",no_argument,NULL,'?'},
 {"http09",no_argument,NULL,'9'},
 {"http10",no_argument,NULL,'1'},
 {"http11",no_argument,NULL,'2'},
 {"get",no_argument,&method,METHOD_GET},
 {"head",no_argument,&method,METHOD_HEAD},
 {"options",no_argument,&method,METHOD_OPTIONS},
 {"trace",no_argument,&method,METHOD_TRACE},
 {"post",required_argument,NULL,'d'},
 {"version",no_argument,NULL,'V'},
 {"proxy",required_argument,NULL,'p'},
 {"clients",required_argument,NULL,'c'},
 {NULL,0,NULL,0}
};

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
   timerexpired=1;
}	

static void usage(void)
{
   fprintf(stderr,
	"webbench [option]... URL\n"
	"  -f|--force               Don't wait for reply from server.\n"
	"  -r|--reload              Send reload request - Pragma: no-cache.\n"
	"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
	"  -p|--proxy <server:port> Use proxy server for request.\n"
	"  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
	"  -9|--http09              Use HTTP/0.9 style requests.\n"
	"  -1|--http10              Use HTTP/1.0 protocol.\n"
	"  -2|--http11              Use HTTP/1.1 protocol.\n"
	"  --get                    Use GET request method.\n"
	"  --head                   Use HEAD request method.\n"
	"  --options                Use OPTIONS request method.\n"
	"  --trace                  Use TRACE request method.\n"
	"  --post                   Use POST request method.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	);
};
int main(int argc, char *argv[])
{
 int opt=0;
 int options_index=0;
 char *tmp=NULL;

 if(argc==1)
 {
	  usage();
          return 2;
 } 

 while((opt=getopt_long(argc,argv,"912Vfrd:t:p:c:?h",long_options,&options_index))!=EOF )
 {
  switch(opt)
  {
   case  0 : break;
   case 'f': force=1;break;
   case 'r': force_reload=1;break; 
   case '9': http10=0;break;
   case '1': http10=1;break;
   case '2': http10=2;break;
   case 'V': printf(PROGRAM_VERSION"\n");exit(0);
   case 't': benchtime=atoi(optarg);break;
   case 'd':
	   	   if(strlen(optarg) >= POSTDATA_SIZE)
	   	   {
	   		   fprintf(stderr,"Error in option --post: post data's length is too long\n");
	   		   return 2;
	   	   }
	   	   strcpy(postData,optarg);
	   	   method = METHOD_POST;
		   sprintf(lengthOfdata,"%d",strlen(postData));
	   	   break;
   case 'p': 
	     /* proxy server parsing server:port */
	     tmp=strrchr(optarg,':');
	     proxyhost=optarg;
	     if(tmp==NULL)
	     {
		     break;
	     }
	     if(tmp==optarg)
	     {
		     fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
		     return 2;
	     }
	     if(tmp==optarg+strlen(optarg)-1)
	     {
		     fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
		     return 2;
	     }
	     *tmp='\0';
	     proxyport=atoi(tmp+1);break;
   case ':':
   case 'h':
   case '?': usage();return 2;break;
   case 'c': clients=atoi(optarg);break;
  }
 }
 
 if(optind==argc) {
                      fprintf(stderr,"webbench: Missing URL!\n");
		      usage();
		      return 2;
                    }

 //重新check两个值
 if(clients==0) clients=1;
 if(benchtime==0) benchtime=60;

 /* Copyright */
 fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
	 "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
	 );

 //生成请求头部
 build_request(argv[optind]);
 /* print bench info */
 printf("\nBenchmarking: ");
 switch(method)
 {
	 case METHOD_GET:
	 default:
		 printf("GET");break;
	 case METHOD_OPTIONS:
		 printf("OPTIONS");break;
	 case METHOD_HEAD:
		 printf("HEAD");break;
	 case METHOD_TRACE:
		 printf("TRACE");break;
	 case METHOD_POST:
		 printf("POST, data are %s", postData);
		 break;
 }
 printf(" %s",argv[optind]);
 switch(http10)
 {
	 case 0: printf(" (using HTTP/0.9)");break;
	 case 2: printf(" (using HTTP/1.1)");break;
 }
 printf("\n");
 if(clients==1) printf("1 client");
 else
   printf("%d clients",clients);

 printf(", running %d sec", benchtime);
 if(force) printf(", early socket close");
 if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
 if(force_reload) printf(", forcing reload");
 printf(".\n");
 return bench();
}

/*
 * 构建请求报文
 */
void build_request(const char *url)
{
  char tmp[10];
  int i;

  /*
   * 设置报文的请求行
   */

  //将host和request初始化，bzero属于bsd标准，而不是posix标准，可以用memset代替
  memset(host,0,sizeof(host));
  memset(request,0,sizeof(request));

  /*
   * 根据METHOD和http版本重新设置连接的http版本
   * http/0.9：已过时，只支持GET一种请求方法
   */
  if(force_reload && proxyhost!=NULL && http10<1) http10=1;	//http/0.9只有GET方法
  if(method==METHOD_HEAD && http10<1) http10=1;	
  if(method==METHOD_OPTIONS && http10<2) http10=2;
  if(method==METHOD_TRACE && http10<2) http10=2;
  if(method==METHOD_POST && http10<1) http10 = 2;

  switch(method)
  {
	  default:
	  case METHOD_GET: strcpy(request,"GET");break;
	  case METHOD_HEAD: strcpy(request,"HEAD");break;
	  case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
	  case METHOD_TRACE: strcpy(request,"TRACE");break;
	  case METHOD_POST: strcpy(request,"POST");break;
  }

  strcat(request," ");

  //合法的url需要protocol://协议信息
  if(NULL==strstr(url,"://"))
  {
	  fprintf(stderr, "\n%s: is not a valid URL.\n",url);
	  exit(2);
  }

  //url长度大于1500时报错，http/1.0rfc中并没有对url的长度有限制，但浏览器和服务器还是有限制的
  if(strlen(url)>1500)
  {
     fprintf(stderr,"URL is too long.\n");
	 exit(2);
  }

  //只支持http, 确定url的协议是http
  if(proxyhost==NULL)
	   if (0!=strncasecmp("http://",url,7))
	   { fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
             exit(2);
           }

  /* protocol/host delimiter */
  i=strstr(url,"://")-url+3;
  /* printf("%d\n",i); */

  if(strchr(url+i,'/')==NULL) {
                                fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
                                exit(2);
                          }
  //webbench不会对proxyhost格式进行检查
  if(proxyhost==NULL)
  {
   /* get port from hostname */
   if(index(url+i,':')!=NULL &&
      index(url+i,':')<index(url+i,'/'))
   {
	   //这有个bug，host的长度为64个字节，可能会溢出
	   strncpy(host,url+i,strchr(url+i,':')-url-i);
	   bzero(tmp,10);

	   //提取端口号，index已过时，可以用strchr替换
	   strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
	   /* printf("tmp=%s\n",tmp); */

	   proxyport=atoi(tmp);
	   //0号端口作为保留端口，这里还可以在端口号大于65535时报错
	   if(proxyport==0) proxyport=80;
   }
   else	//如果指定代理服务器，直接将url存储host，不检查
   {
     strncpy(host,url+i,strcspn(url+i,"/"));
   }
   // printf("Host=%s\n",host);
   strcat(request+strlen(request),url+i+strcspn(url+i,"/"));
  } else
  {
   // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
   strcat(request,url);
  }
  if(http10==1)
	  strcat(request," HTTP/1.0");
  else if (http10==2)
	  strcat(request," HTTP/1.1");
  strcat(request,"\r\n");
  if(http10>0)
	  strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");
  if(proxyhost==NULL && http10>0)
  {
	  strcat(request,"Host: ");
	  strcat(request,host);
	  strcat(request,"\r\n");
  }

  /*
   * 至此，报文的请求行已经设置完成，接下来设置首部行
   */

  if(force_reload && proxyhost!=NULL)
  {
	  strcat(request,"Pragma: no-cache\r\n");
  }

  //关闭keep-alive
  if(http10>1)
	  strcat(request,"Connection: close\r\n");

  //增加post data
  if(method==METHOD_POST)
  {
	  strcat(request,"Content-Length: ");
	  strcat(request,lengthOfdata);
	  strcat(request,"\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n");
	  strcat(request,postData);
	  strcat(request,"\r\n");
  }

  /*
   * 首部行设置完成，添加空行
   */
  /* add empty line at end */
  if(http10>0) strcat(request,"\r\n");
  /* printf("Req=%s\n",request);*/
}

/* vraci system rc error kod */
static int bench(void)
{
	int i,j,k;
	pid_t pid=0;
	FILE *f;

	//先连接一次，看是否能连接成功
	/* check avaibility of target server */
	i=Socket(proxyhost==NULL?host:proxyhost,proxyport);
	if(i<0) {
		fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
		return 1;
	}
	close(i);

	//利用管道来传输数据
	/* create pipe */
	if(pipe(mypipe))
	{
		perror("pipe failed.");
		return 3;
	}

	/* not needed, since we have alarm() in childrens */
	/* wait 4 next system clock tick */
	/*
	cas=time(NULL);
	while(time(NULL)==cas)
	sched_yield();
	*/

	/* fork childs */
	for(i=0;i<clients;i++)
	{
		pid=fork();
		if(pid <= (pid_t) 0)//是子进程或出错
		{
			/* child process or error*/
			sleep(1); /* make childs faster */ /*why?*/
			break;
		}
	}

	//fork失败
	if( pid< (pid_t) 0)
	{
		fprintf(stderr,"problems forking worker no. %d\n",i);
		perror("fork failed.");
		return 3;
	}

	//子进程
	if(pid== (pid_t) 0)
	{
		/* I am a child */
		if(proxyhost==NULL)
			benchcore(host,proxyport,request);
		else
			benchcore(proxyhost,proxyport,request);

		/* write results to pipe */
		f=fdopen(mypipe[1],"w");	//向管道写数据
		if(f==NULL)
		{
			perror("open pipe for writing failed.");
			return 3;
		}
		/* fprintf(stderr,"Child - %d %d\n",speed,failed); */
		fprintf(f,"%d %d %d\n",speed,failed,bytes);
		fclose(f);
		return 0;
	} else
	{
		//父进程，通过管道读数据
		f=fdopen(mypipe[0],"r");
		if(f==NULL)
		{
			perror("open pipe for reading failed.");
			return 3;
		}
		//不设置缓冲
		setvbuf(f,NULL,_IONBF,0);
		speed=0;
		failed=0;
		bytes=0;

		//从管道中读数据
		while(1)
		{
			pid=fscanf(f,"%d %d %d",&i,&j,&k);
			if(pid<2)
			{
				fprintf(stderr,"Some of our childrens died.\n");
				break;
			}
			speed+=i;
			failed+=j;
			bytes+=k;
			/* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
			if(--clients==0) break;
		}
		fclose(f);
		printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
		(int)((speed+failed)/(benchtime/60.0f)),
		(int)(bytes/(float)benchtime),
		speed,
		failed);
	}
	return i;
}

void benchcore(const char *host,const int port,const char *req)
{
	int rlen;	//req字节数
	char buf[1500];	//接收数据的缓冲区
	int s,i;	//s:socket fd; i:读取的字节数
	struct sigaction sa;	//信号处理

	/* setup alarm signal handler */
	sa.sa_handler=alarm_handler;
	sa.sa_flags=0;
	if(sigaction(SIGALRM,&sa,NULL))
		exit(3);
	alarm(benchtime);

	//请求报文字节数
	rlen=strlen(req);
	nexttry:while(1)
	{
		if(timerexpired)
		{
			if(failed>0)
			{
				/* fprintf(stderr,"Correcting failed by signal\n"); */
				failed--;
			}
			return;
		}
		s=Socket(host,port);

		if(s<0) { failed++;continue;}

		//向socket中写入数据，write函数写成功则返回写入的数据量；失败返回-1
		if(rlen!=write(s,req,rlen)) {failed++;close(s);continue;}
		if(http10==0)
			if(shutdown(s,1)) { failed++;close(s);continue;}

		//等待服务器响应
		if(force==0)
		{
			/* read all available data from socket */
			while(1)
			{
				if(timerexpired) break;
				i=read(s,buf,1500);
				/* fprintf(stderr,"%d\n",i); */
				/*	printf("%s",buf);	*/
				if(i<0)
				{
					failed++;
					close(s);
					goto nexttry;
				}
				else
					if(i==0) break;
				else
					bytes+=i;
			}
		}
		if(close(s)) {failed++;continue;}
		speed++;
	}
}
