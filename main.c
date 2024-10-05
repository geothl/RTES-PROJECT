#include <libwebsockets.h>
#include <signal.h>
#include <cjson/cJSON.h> 
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define QUEUESIZE 100

static struct lws_context *cx;
static int interrupted;
int connected=0;
//struct lws_ss_handle * h;
struct lws_context_creation_info info;
struct lws_client_connect_info i;
int flag_error=0;
int N=4; //4 symbols
int index_period=0;
int end=0;
//int err_429=0;
long long int prev_time=0;
long long int after_time=0;
long long int diff_time=0;
int fatal_error=0;
int wait=1;
int count_timer=0;

int end_var=0;

pthread_mutex_t mutex_prod;
pthread_mutex_t mutex_candle;
pthread_mutex_t mutex_period;

//pthread_mutexattr_t attr;


pthread_cond_t cond_period;


//paths for file writing in raspberry pi ssd card
const char *symbol_array[] = {"TSLA","COINBASE:ETH-USD","AAPL","AMZN"};
const char *symbol_path[]={"./files/tesla.txt","./files/ethereum.txt","./files/apple.txt","./files/amazon.txt"};
const char * candle_path[]={"./files/tesla_candle.txt","./files/ethereum_candle.txt","./files/apple_candle.txt","./files/amazon_candle.txt"};
const char * period_path[]={"./files/tesla_period.txt","./files/ethereum_period.txt","./files/apple_period.txt","./files/amazon_period.txt"};


struct lws *wsi=NULL;

FILE *fptr_trades[4];
FILE *fptr_candle[4];
FILE *fptr_period[4];

//struct for trade data
typedef struct {
  long long int timestamp_trade;
  long long int timestamp_receive;
  char symbol[100];
  double volume;
  double price;
} trade;

//struct for queue
typedef struct {
  trade buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

//candlestick processing inside minute interval struct
typedef struct {
  float opening_price;
  float closing_price;
  float max_price;
  float min_price;
  float volume;
  float sum;
  int n;
  int b;
} candlestick;

candlestick cd[4];  //one candlestick cd for every symbol
float mean_period_arr[4][15]; //one circular array for 15 minute moving average calculation
float volume_period_arr[4][15];

//ordinary queue functions used in C and common open libraries. This implemenation is 
static queue *fifo;
queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, long long int timestamp_trade,long long int timestamp_receive,char * symbol,double volume,double price);
void queueDel (queue *q, trade *out);

void closing_routine(int sig);
void timer_handler(int signum);
long long int Current_Timestamp();
void initialize_files();
void read_received_data(void *in);
int symbol_identity(const char * symbol);
void write_data(trade *trd,int i);
static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                    void *user, void *in, size_t len);
void info_creation(int argc, const char **argv);
void connect_ws ();
void *producer ();
void *consumer ();
void *minute_write();

static const struct lws_protocols protocols[] = {
    {
        .name = "http",
        .callback = callback,  // Register the callback function here
        .per_session_data_size = 0,
        .rx_buffer_size = 0,
    },
    { NULL, NULL, 0, 0 } // Terminator
};


static void
sigint_handler(int sig)
{
	lws_default_loop_exit(cx);
}



int main(int argc, const char **argv)
{
  struct sigaction sa;
  struct itimerval timer;

    // Install timer_handler as the signal handler for SIGALRM
  sa.sa_handler = &timer_handler;
  sa.sa_flags = SA_RESTART; // Automatically restart interrupted system calls
  sigaction(SIGALRM, &sa, NULL);

    // Configure the timer to expire after 1 second, then every 1 second
  timer.it_value.tv_sec = 60;      // Initial delay before first trigger
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 60;   // Interval between subsequent triggers
  timer.it_interval.tv_usec = 0;

  // Start the timer that will trigger the timer_handler function that will wake up the minute_write thread
  setitimer(ITIMER_REAL, &timer, NULL);

  //set up ctrl+c to call the closing_routine
  signal(SIGINT, closing_routine);

  pthread_t pro,mw;
  pthread_t consumers[3];
  //create files and column titles
  initialize_files();
  for (int i=0;i<N;i++){  //initizalize candlestick struct for the first minute
  cd[i].b=1;  
  }
  //create queue 
  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }
  
  
  //create mutexes
  pthread_mutex_init(&mutex_candle,NULL);
  pthread_mutex_init(&mutex_period,NULL);
  pthread_cond_init(&cond_period,NULL);
  //create info for connection
  info_creation(argc, argv);
  //creat threads
  pthread_create (&pro, NULL, producer, NULL);
  for (long i = 0; i < 3; i++) {
    pthread_create(&consumers[i], NULL, consumer, NULL);
  }
  pthread_create (&mw, NULL, minute_write, NULL);
  //join threads
  pthread_join (pro, NULL);
  pthread_join (mw, NULL);
  for (int i = 0; i < 3; i++) {
        pthread_join(consumers[i], NULL);
  }
  //once ctrl+c is pressed and all threads have returned we close the file pointers and release the memory that was allocated to the queue
     for(int i=0;i<N;i++){
      fclose(fptr_trades[i]);
      fclose(fptr_candle[i]);
      fclose(fptr_period[i]);
    }
  queueDelete (fifo);
  printf("Resources and memory freed. Closing program...\n");
  return 0;
}



queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, long long int timestamp_trade,long long int timestamp_receive,char * symbol,double volume,double price)
{
  (q->buf[q->tail]).timestamp_trade = timestamp_trade;
  (q->buf[q->tail]).timestamp_receive = timestamp_receive;
  (q->buf[q->tail]).volume = volume;
  (q->buf[q->tail]).price = price;
  strcpy((q->buf[q->tail]).symbol,symbol) ;

  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, trade *out)
{
  out->timestamp_trade = (q->buf[q->head]).timestamp_trade;
  out->timestamp_receive = (q->buf[q->head]).timestamp_receive;
  out->volume = (q->buf[q->head]).volume;
  out->price = (q->buf[q->head]).price;
  strcpy(out->symbol,(q->buf[q->head]).symbol) ;
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}
// Activated through SIGINT (Ctrl+C). Opens this closing_routine to enable the system to close after safely freeing allocated memory and the thread process has returned.
void closing_routine(int sig) {
    end_var=1;   //set flag to 1 so all threads can start their closing procedure 
    pthread_cond_broadcast(&cond_period); 
    pthread_cond_broadcast(fifo->notFull);   //we broadcast the conditions so waiting threads can exit
    pthread_cond_broadcast(fifo->notEmpty);
}

//This function is triggered every minute by interrupt signal from timer
void timer_handler(int signum) { 
    count_timer++;   //update minute counter
    pthread_mutex_lock(&mutex_period);  
    wait=0;
    pthread_cond_signal(&cond_period);  //signal minute write function to wake up
    pthread_mutex_unlock(&mutex_period);
    
}



long long int Current_Timestamp(){ // Get unix timestamp for local time.
   
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long int milliseconds = ((long long int)tv.tv_sec * 1000) + ((long long int)tv.tv_usec / 1000);
   return milliseconds;
}


void initialize_files(){ //clean file/open it so new data can be collected with every run.
    for (int i=0;i<N;i++){
 fptr_trades[i] = fopen(symbol_path[i], "w");
  fprintf(fptr_trades[i], "%s ; %s ; %s ;%s ; %s ; %s \n","timestamp_trade","timestamp_receive","timestamp_written","symbol","volume","price");
  //fclose(fptr);
  fptr_candle[i] = fopen(candle_path[i],"w");
  fprintf(fptr_candle[i], "%s;%s;%s;%s;%s;\n","opening price","closing price","max price","min price","total volume");
  //fclose(fptr);
  fptr_period[i]= fopen(period_path[i], "w");
  fprintf(fptr_period[i], "%s;%s;\n","mean price","total volume");
 // fclose(fptr);
    }

}

void read_received_data(void *in){ //receive and parse JSON data from finnhub. If it was trading data add to queue in order to be processed.
            if(end_var==1){ //check if it is time to return
                   return ;
             }
            long long int time_received=Current_Timestamp(); //get timestamp for received data
            cJSON *json = cJSON_Parse((const char *)in); //start parsing the JSON.
            cJSON *data = cJSON_GetObjectItemCaseSensitive(json,"data");
            cJSON *type = cJSON_GetObjectItemCaseSensitive(json,"type");
            if( cJSON_IsString(type)){   //if we have {type:"error"} message change connected flag to 0 so , the producer can destroy the current connection and create a new one.
            if(!strcmp(type->valuestring,"error")){
                connected=0;
                cJSON_Delete(json);
                lwsl_err("Error");
                //flag_error=1;
                return;
            }
            }
            int arr_size=1;
            cJSON *data_index=NULL;
            arr_size=cJSON_GetArraySize(data);
            for (int i=0;i<arr_size;i++){ //parsing procedure
            data_index=cJSON_GetArrayItem(data,i);
            // Extract data from JSON (example)
            cJSON *symbol = cJSON_GetObjectItemCaseSensitive(data_index,"s");
            cJSON *price = cJSON_GetObjectItemCaseSensitive(data_index,"p");
            cJSON *volume = cJSON_GetObjectItemCaseSensitive(data_index,"v");
            cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(data_index,"t");

            if (cJSON_IsNumber(volume) && cJSON_IsString(symbol) && cJSON_IsNumber(price)&&cJSON_IsNumber(timestamp)) {//if trade data in correct format
             pthread_mutex_lock (fifo->mut);
            while (fifo->full) {
             pthread_cond_wait (fifo->notFull, fifo->mut);
               if(end_var==1){//return if ctrl+c is pressed
                   return ;
             }
             }
            queueAdd (fifo, (long long int) (long long int)timestamp->valuedouble,time_received,symbol->valuestring,volume->valuedouble,price->valuedouble); //add data received trade data to queue
        
            pthread_mutex_unlock (fifo->mut);
            pthread_cond_signal (fifo->notEmpty);   
            
            }
            }
           // }          
                 cJSON_Delete(json); //free allocated memory. also frees all dependent allocation to root
                  
                return;

}

//This functions checks the indentity of the symbol and retuns i index of the received symbol in symbol_array.
int symbol_identity(const char * symbol){  
   int i=-1;
   if(!strcmp(symbol,symbol_array[0])){
            i=0;

    }
    else if (!strcmp(symbol,symbol_array[1])){
            i=1;
    }
       else if (!strcmp(symbol,symbol_array[2])){
            i=2;
    }
    else if (!strcmp(symbol,symbol_array[3])){
            i=3;
    }


    return i;
}

//write received trade data to memory
void write_data(trade *trd,int i){ 
fprintf(fptr_trades[i], "%lld ; %lld ; %lld; %s ; %f ; %f \n",trd->timestamp_trade,trd->timestamp_receive,Current_Timestamp(),trd->symbol,trd->volume,trd->price);
}

//callback function for the producer-client procedure
static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                    void *user, void *in, size_t len) {
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("WebSocket connection established\n");
            //array of subscription requests
            char * str[4]={"{\"type\":\"subscribe\",\"symbol\":\"TSLA\"}" ,"{\"type\":\"subscribe\",\"symbol\":\"COINBASE:ETH-USD\"}","{\"type\":\"subscribe\",\"symbol\":\"AAPL\"}","{\"type\":\"subscribe\",\"symbol\":\"AMZN\"}" };
            //submit subscription request through buffer and lws_write in websocket
            for (int i=0; i<N; i++){
            char *message = str[i];
            size_t message_length = strlen(message);
             // we allocate a buffer that includes LWS_PRE and the actual message
            size_t buffer_size = LWS_PRE + message_length;
            unsigned char *buffer = (unsigned char *)malloc(buffer_size);
            // Pointer to where the actual data-message will be placed after LWS_PRE
            unsigned char *payload = buffer + LWS_PRE;
            // Copy the message after LWS_PRE bytes
            memcpy(payload, message, message_length);
            // Use lws_write to send the message through the socket
            int written = lws_write(wsi, payload, message_length, LWS_WRITE_TEXT);
             // Free the buffer after usage
                free(buffer);
            }    
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: //receiveing data-stream though socket handling
            read_received_data( in);
            if(flag_error==1){
                flag_error=0;    
                return -1;
            }
            break;

        case LWS_CALLBACK_CLIENT_CLOSED: //for forced collection
            printf("WebSocket connection closed\n");
            connected=0;
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: //in case of error during connection
            printf("WebSocket connection error\n");
            connected=0;
            break;

        default: //do nothing
            break;
    }

    return 0;
}

//create info one time at the begining of the program that will be used to create every connection
void info_creation(int argc, const char **argv){
 memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.protocols = protocols;  // Include the protocols with callback
    lws_cmdline_option_handle_builtin(argc, argv, &info);
}

//the connection-client connection procedure through the producer
void connect_ws (){
    
	cx = lws_create_context(&info); //creation of connection CONTEXT=cx
	if (!cx) {
		lwsl_err("lws init failed\n");
		
	}
  //i is the lws_client_connect_info struct through which we will create the connection context
    memset(&i, 0, sizeof(i));
    i.context = cx;
    i.address = "ws.finnhub.io";
    i.port = 443;
    i.path = "/?token=cq9cmq9r01qlu7f2fhhgcq9cmq9r01qlu7f2fhi0"; //my api key
    i.host = i.address;
    i.origin = i.address;
    i.protocol = "http";
    i.ssl_connection = LCCSCF_USE_SSL; //secure safe connection
    wsi = lws_client_connect_via_info(&i);  //CREATION OF WEBSOCKET

	if (!wsi) {
		lwsl_err("failed to create secure stream");
		interrupted = 1;
	}
    else{
        lwsl_user("Connection");
        connected=1;
    }


}

//The sole producer thread
void *producer ()
{  
    while(1){
    if(end_var==1){
        return NULL;
    }
    connect_ws();  //connect-create client
    //
    while ((lws_service(cx, 1000) >= 0)&&(connected==1)) { 
       if(end_var==1){
        lws_context_destroy(cx);
        return NULL;
        }
    }

   	lws_context_destroy(cx); //error,disconnection or websocket corruption. We destroy the context in order to create a new one and a new websocekt.
    }
  return (NULL);
}

//The consumer procedure
void *consumer ()
{ 
  trade trd; //trade struct that will hold the trades key items

  while(1){
    pthread_mutex_lock (fifo->mut); 
      if(end_var==1){
        pthread_mutex_unlock (fifo->mut);
      return NULL;
    }
    while (fifo->empty) {
      pthread_cond_wait (fifo->notEmpty, fifo->mut); //wait if empty queue
      if(end_var==1){
        pthread_mutex_unlock (fifo->mut);
        return NULL;
      }
    }
    queueDel (fifo, &trd); //consume already parsed trade data from producer that was in the queue
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);
    int i=symbol_identity(trd.symbol); //indentify which symbol is the received trade from
    write_data(&trd,i); //write the data about received trade to file
    pthread_mutex_lock(&mutex_candle);  //LOCK MUTEX to calculate the candlestick real time value before the minute write.
    if(cd[i].b==1){ //if first trade for symbol after minute write then 
    cd[i].opening_price=trd.price;    
    cd[i].closing_price=trd.price; 
    cd[i].max_price=trd.price;
    cd[i].min_price=trd.price;
    cd[i].b=0; //next trade won't be the first one in the one minute interval
    }
    else{
        if (cd[i].max_price<trd.price){
             cd[i].max_price=trd.price;
        }
         if(cd[i].min_price>trd.price){
            cd[i].min_price=trd.price;
        }
    }
    cd[i].closing_price=trd.price; 
    cd[i].volume+=trd.volume; //calculate the sum of the volume of trades in the minute interval
    cd[i].sum+=trd.price; //in order to calculate mean in minute interval
    cd[i].n++;
    
    pthread_mutex_unlock(&mutex_candle);

  }

  return (NULL);
}


//procedure for the thread that writes on candlestick file and moving_average+total_volume=_period file every minute 
void *minute_write(){
float mean_period=0;
float total_volume=0; 
while(1){

pthread_mutex_lock(&mutex_period);
while(wait){
pthread_cond_wait(&cond_period ,&mutex_period); //wait for wake up signal from timer_handler
if(end_var==1){
pthread_mutex_unlock(&mutex_period);
return NULL;
}

}
//we lock the mutex_candle to avoid data_racing with the cd live 
pthread_mutex_lock(&mutex_candle);
    for(int i=0;i<N;i++){ //write candlestick in candlestick file every minute
          fprintf(fptr_candle[i], "%f;%f;%f;%f;%f;\n",cd[i].opening_price,cd[i].closing_price,cd[i].max_price,cd[i].min_price,cd[i].volume);
          cd[i].opening_price=cd[i].closing_price;
          cd[i].max_price=cd[i].closing_price;
          cd[i].min_price=cd[i].closing_price;
          volume_period_arr[i][index_period]=cd[i].volume;
          if(cd[i].n>0){
              mean_period_arr[i][index_period]=cd[i].sum/cd[i].n;
          }
          else{
          mean_period_arr[i][index_period]=0;  
          }
          cd[i].volume=0;
          cd[i].sum=0;
          cd[i].n=0;
          cd[i].b=1;
    
    }
    if(end<15){ //in order to have correct estimates in the first 15 minutes of the program
          end++;    
    }

pthread_mutex_unlock(&mutex_candle);

//get timestamp in order to see the time difference between every minute write of candlestick file and moving_average+volume=_period file. This way we know how much later than the expected time the perioding write takes place.
//the program is structed in a way that we keep the timestamp of the first minute write and afterwards only the time diffreneces.
after_time=Current_Timestamp();
diff_time=after_time-prev_time;

for(int i=0;i<N;i++){
for(int c=0;c<end;c++){
mean_period=mean_period+mean_period_arr[i][c]; //calculate by using the Circular Queue
total_volume=total_volume+volume_period_arr[i][c];
}
mean_period=mean_period/end;

//write to file with moving average and total volume in 15 minutes
fprintf(fptr_period[i], "%f;%f;%lld\n",mean_period,total_volume,diff_time);
mean_period=0;
total_volume=0;
}
prev_time=after_time; //keep timestamp in order to calculate time differnce the next time the thread wake up

index_period=(index_period+1)%15; //get ready for the next
printf("Candlesticks and MA&Volume files created for minute %d\n",count_timer);    
wait=1;
pthread_mutex_unlock(&mutex_period);
}
return NULL;
}



