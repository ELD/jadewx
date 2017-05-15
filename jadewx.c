#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <curl/curl.h>
#include <mysql/mysql.h>

char *mysql_username=NULL,*mysql_password=NULL;
char *WU_station=NULL,*WU_password=NULL;
int deviceID;
int comm_interval=10;
int config_set=0,config_requested=0;
unsigned char cfg_cs[2]={0,0};
const size_t RX_BUFFER_LENGTH=21;
unsigned char *RX_buffer;
const size_t TX_BUFFER_LENGTH=21;
unsigned char *TX_buffer;
unsigned char *state_buffer=NULL,*msg_buffer=NULL;
const size_t CONFIG_BUFFER_LENGTH=51;
unsigned char *config_buffer=NULL;
const char *URL_FORMAT="https://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=%s&PASSWORD=%s&action=updateraw&realtime=1&rtfreq=2.5&softwaretype=homegrown&winddir=%d&windspeedmph=%.1f&windgustmph=%.1f&humidity=%d&dewptf=%.1f&tempf=%.1f&baromin=%.2f&rainin=%.2f&dailyrainin=%.2f&dateutc=%04d-%02d-%02d+%02d%%3A%02d%%3A%02d";
char *url=NULL;
int mdays[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
struct timespec first_sleep,next_sleep;
int latest_haddr=0xfffff;
float rain_day=-999.;

void initialize_transceiver(libusb_device_handle *handle,char *serial_num)
{
  printf("initializing transceiver...\n");
  unsigned char *buffer=(unsigned char *)malloc(21*sizeof(unsigned char));
  for (size_t n=0; n < 21; ++n) {
    buffer[n]=0xcc;
  }
  buffer[0]=0xdd;
  buffer[1]=0x0a;
  buffer[2]=0x1;
  buffer[3]=0xf5;
  int status=libusb_control_transfer(handle,0x21,0x9,0x3dd,0,buffer,21,1000);
  status=libusb_control_transfer(handle,0xa1,0x1,0x3dc,0,buffer,21,1000);
for (size_t n=0; n < 21; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  int corr_val=(((buffer[4] << 8) | buffer[5]) << 8 | buffer[6]) << 8 | buffer[7];
  int adj_freq=(905000000./16000000.*16777216.)+corr_val;
  if ( (adj_freq % 2) == 0) {
    ++adj_freq;
  }
printf("corr vals: %d %d %d %d  adj: %d  adj freq: %d\n",(int)buffer[4],(int)buffer[5],(int)buffer[6],(int)buffer[7],corr_val,adj_freq);

  buffer[2]=0x1;
  buffer[3]=0xf9;
  status=libusb_control_transfer(handle,0x21,0x9,0x3dd,0,buffer,21,1000);
  status=libusb_control_transfer(handle,0xa1,0x1,0x3dc,0,buffer,21,1000);
for (size_t n=0; n < 21; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  sprintf(serial_num,"%02d%02d%02d%02d%02d%02d%02d",(int)buffer[4],(int)buffer[5],(int)buffer[6],(int)buffer[7],(int)buffer[8],(int)buffer[9],(int)buffer[10]);
  deviceID=(buffer[9] << 8) | buffer[10];
printf("serial num: %s  deviceID: %d (%04x)\n",serial_num,deviceID,deviceID);

  int addr[50],rval[50];
// IFMODE
  addr[0]=0x8;
  rval[0]=0x0;
// MODULATION
  addr[1]=0x10;
  rval[1]=0x41;
// ENCODING
  addr[2]=0x11;
  rval[2]=0x7;
// FRAMING
  addr[3]=0x12;
  rval[3]=0x84;
// CRCINIT0
  addr[4]=0x17;
  rval[4]=0xff;
// CRCINIT1
  addr[5]=0x16;
  rval[5]=0xff;
// CRCINIT2
  addr[6]=0x15;
  rval[6]=0xff;
// CRCINIT3
  addr[7]=0x14;
  rval[7]=0xff;
// FREQ0
  addr[8]=0x23;
  rval[8]=(adj_freq >> 0) & 0xff;
// FREQ1
  addr[9]=0x22;
  rval[9]=(adj_freq >> 8) & 0xff;
// FREQ2
  addr[10]=0x21;
  rval[10]=(adj_freq >> 16) & 0xff;
// FREQ3
  addr[11]=0x20;
  rval[11]=(adj_freq >> 24) & 0xff;
printf("%x %x %x %x\n",(int)rval[8],(int)rval[9],(int)rval[10],(int)rval[11]);
// FSKDEV2
  addr[12]=0x25;
  rval[12]=0x0;
// FSKDEV1
  addr[13]=0x26;
  rval[13]=0x31;
// FSKDEV0
  addr[14]=0x27;
  rval[14]=0x27;
// IFFREQHI
  addr[15]=0x28;
  rval[15]=0x20;
// IFFREQLO
  addr[16]=0x29;
  rval[16]=0x0;
// PLLLOOP
  addr[17]=0x2c;
  rval[17]=0x1d;
// PLLRANGING
  addr[18]=0x2d;
  rval[18]=0x8;
// PLLRNGCLK
  addr[19]=0x2e;
  rval[19]=0x3;
// TXPWR
  addr[20]=0x30;
  rval[20]=0x3;
// TXRATEHI
  addr[21]=0x31;
  rval[21]=0x0;
// TXRATEMID
  addr[22]=0x32;
  rval[22]=0x51;
// TXRATELO
  addr[23]=0x33;
  rval[23]=0xec;
// MODMISC
  addr[24]=0x34;
  rval[24]=0x3;
// ADCMISC
  addr[25]=0x38;
  rval[25]=0x1;
// AGCTARGET
  addr[26]=0x39;
  rval[26]=0xe;
// AGCATTACK
  addr[27]=0x3a;
  rval[27]=0x11;
// AGCDECAY
  addr[28]=0x3b;
  rval[28]=0xe;
// CICDEC
  addr[29]=0x3f;
  rval[29]=0x3f;
// DATARATEHI
  addr[30]=0x40;
  rval[30]=0x19;
// DATARATELO
  addr[31]=0x41;
  rval[31]=0x66;
// TMGGAINHI
  addr[32]=0x42;
  rval[32]=0x1;
// TMGGAINLO
  addr[33]=0x43;
  rval[33]=0x96;
// PHASEGAIN
  addr[34]=0x44;
  rval[34]=0x3;
// FREQGAIN
  addr[35]=0x45;
  rval[35]=0x4;
// FREQGAIN2
  addr[36]=0x46;
  rval[36]=0xa;
// AMPLGAIN
  addr[37]=0x47;
  rval[37]=0x6;
// SPAREOUT
  addr[38]=0x60;
  rval[38]=0x0;
// TESTOBS
  addr[39]=0x68;
  rval[39]=0x0;
// APEOVER
  addr[40]=0x70;
  rval[40]=0x0;
// TMMUX
  addr[41]=0x71;
  rval[41]=0x0;
// PLLVCOI
  addr[42]=0x72;
  rval[42]=0x1;
// PLLCPEN
  addr[43]=0x73;
  rval[43]=0x1;
// AGCMANUAL
  addr[44]=0x78;
  rval[44]=0x0;
// ADCDCLEVEL
  addr[45]=0x79;
  rval[45]=0x10;
// RFMISC
  addr[46]=0x7a;
  rval[46]=0xb0;
// TXDRIVER
  addr[47]=0x7b;
  rval[47]=0x88;
// REF
  addr[48]=0x7c;
  rval[48]=0x23;
// RXMISC
  addr[49]=0x7d;
  rval[49]=0x35;
printf("%d %d %d %d\n",rval[11],rval[10],rval[9],rval[8]);
  unsigned char *reg_buffer=(unsigned char *)malloc(5*sizeof(unsigned char));
  reg_buffer[0]=0xf0;
  reg_buffer[2]=0x1;
  reg_buffer[4]=0;
  for (size_t n=0; n < 50; ++n) {
// address
    reg_buffer[1]=addr[n] & 0x7f;
// data to write
    reg_buffer[3]=rval[n];
    int status=libusb_control_transfer(handle,0x21,0x9,0x3f0,0,reg_buffer,5,1000);
  }
  printf("...transceiver initialization done\n");
}

void setRX(libusb_device_handle *handle)
{
  if (RX_buffer == NULL) {
    RX_buffer=(unsigned char *)malloc(RX_BUFFER_LENGTH*sizeof(unsigned char));
    RX_buffer[0]=0xd0;
    for (size_t n=1; n < RX_BUFFER_LENGTH; ++n) {
	RX_buffer[n]=0x0;
    }
  }
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d0,0,RX_buffer,21,1000);
  printf("set RX status: %d\n",status);
  for (size_t n=0; n < RX_BUFFER_LENGTH; ++n) {
    printf(" %02x",RX_buffer[n]);
  }
  printf("\n");
}

void do_setup(libusb_device_handle *handle)
{
  unsigned char *buffer=(unsigned char *)malloc(21*sizeof(unsigned char));
  for (size_t n=0; n < 21; ++n) {
    buffer[n]=0x0;
  }
// execute
  buffer[0]=0xd9;
  buffer[1]=0x5;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d9,0,buffer,15,1000);
printf("execute status: %d\n",status);
// set preamble pattern
  buffer[0]=0xd8;
  buffer[1]=0xaa;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d8,0,buffer,21,1000);
printf("set preamble status: %d\n",status);
// set state
  buffer[0]=0xd7;
  buffer[1]=0x0;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d7,0,buffer,21,1000);
printf("set state status: %d\n",status);
  sleep(1);
// setRX
  setRX(handle);
// set preamble pattern
  buffer[0]=0xd8;
  buffer[1]=0xaa;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d8,0,buffer,21,1000);
printf("set preamble status: %d\n",status);
// set state
  buffer[0]=0xd7;
  buffer[1]=0x1e;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d7,0,buffer,21,1000);
printf("set state status: %d\n",status);
  sleep(1);
// setRX
  setRX(handle);
  first_sleep.tv_sec=0;
  first_sleep.tv_nsec=85000000;
  next_sleep.tv_sec=0;
  next_sleep.tv_nsec=5000000;
}

void setTX(libusb_device_handle *handle)
{
// setTX
  if (TX_buffer == NULL) {
    TX_buffer=(unsigned char *)malloc(TX_BUFFER_LENGTH*sizeof(unsigned char));
    TX_buffer[0]=0xd1;
    for (size_t n=1; n < TX_BUFFER_LENGTH; ++n) {
	TX_buffer[n]=0x0;
    }
  }
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d1,0,TX_buffer,21,1000);
  printf("setTX status %d \n",status);
  for (size_t n=0; n < TX_BUFFER_LENGTH; ++n) {
    printf(" %02x",TX_buffer[n]);
  }
  printf("\n");
}

MYSQL mysql;
void open_mysql()
{
  if (mysql_init(&mysql) == NULL) {
    fprintf(stderr,"Error initializing MySQL\n");
    exit(1);
  }
  if (!mysql_real_connect(&mysql,NULL,mysql_username,mysql_password,NULL,0,NULL,0)) {
    fprintf(stderr,"Error connecting to the database\n");
    exit(1);
  }
}

void close_mysql()
{
  mysql_close(&mysql);
}

int get_state(libusb_device_handle *handle)
{
  if (state_buffer == NULL) {
    state_buffer=(unsigned char *)malloc(10*sizeof(unsigned char));
  }
  state_buffer[1]=0x0;
  int status=libusb_control_transfer(handle,0xa1,0x1,0x3de,0,state_buffer,10,1000);
//printf("getstate status: %d %x %x %x %x %x %x\n",status,state_buffer[0],state_buffer[1],state_buffer[2],state_buffer[3],state_buffer[4],state_buffer[5]);
  return state_buffer[1];
}

void request_set_config(libusb_device_handle *handle,unsigned char *buffer,int first)
{
  buffer[0]=0xd5;
  buffer[1]=0x0;
  buffer[2]=0x9;
  if (first) {
    buffer[3]=0xf0;
    buffer[4]=0xf0;
  }
  else {
    buffer[3]=((deviceID >> 8) & 0xff);
    buffer[4]=(deviceID & 0xff);
  }
  buffer[5]=0x2;
  buffer[6]=cfg_cs[0];
  buffer[7]=cfg_cs[1];
  buffer[8]=(comm_interval >> 4) & 0xff;
  int history_addr=0xffffff;
  buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
  buffer[10]=(history_addr >> 8) & 0xff;
  buffer[11]=(history_addr >> 0) & 0xff;
  printf("request-set-config");
  for (size_t n=0; n < buffer[2]+3; ++n) {
    printf(" %02x",buffer[n]);
  }
  printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,buffer,12,1000);
}

void request_weather_message(libusb_device_handle *handle,int action,int history_addr)
{
  if (msg_buffer == NULL) {
    msg_buffer=(unsigned char *)malloc(12*sizeof(unsigned char));
  }
  msg_buffer[0]=0xd5;
  msg_buffer[1]=0x0;
  msg_buffer[2]=0x9;
  msg_buffer[3]=((deviceID >> 8) & 0xff);
  msg_buffer[4]=(deviceID & 0xff);
  msg_buffer[5]=action;
  msg_buffer[6]=cfg_cs[0];
  msg_buffer[7]=cfg_cs[1];
  msg_buffer[8]=(comm_interval >> 4) & 0xff;
  msg_buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
  msg_buffer[10]=(history_addr >> 8) & 0xff;
  msg_buffer[11]=(history_addr >> 0) & 0xff;
  for (size_t n=0; n < 12; ++n) {
    printf(" %02x",(int)msg_buffer[n]);
  }
  printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,msg_buffer,12,1000);
}

void swap(unsigned char *buffer,int start,int stop)
{
  size_t mid=(stop-start+1)/2;
  size_t m=stop;
  for (size_t n=0; n < mid; ++n) {
    unsigned char temp=buffer[start+n];
    buffer[start+n]=buffer[m];
    buffer[m]=temp;
    --m;
  }
}

void store_config(libusb_device_handle *handle,unsigned char *buffer)
{
  printf("***GETCONFIG***\n");
  int ocs=7;
  for (size_t n=0; n < CONFIG_BUFFER_LENGTH; ++n) {
    printf(" %02x",buffer[n]);
    if (n >= 7 && n < 47) {
	ocs+=buffer[n];
    }
  }
  printf("\nold checksum: %x\n",ocs);
  if (!config_set) {
    if (config_buffer == NULL) {
	config_buffer=(unsigned char *)malloc(CONFIG_BUFFER_LENGTH*sizeof(unsigned char));
    }
    memcpy(config_buffer,buffer,CONFIG_BUFFER_LENGTH);
    config_buffer[0]=0xd5;
    swap(config_buffer,14,18);
    swap(config_buffer,19,23);
    swap(config_buffer,24,25);
    swap(config_buffer,26,27);
    swap(config_buffer,28,31);
    swap(config_buffer,33,35);
    swap(config_buffer,36,40);
    swap(config_buffer,41,45);
    config_buffer[9]=0x4;
// set history interval to 1 minute
    config_buffer[32]=0x0;
    config_buffer[46]=0x10;
    int cs=7;
    for (size_t n=7; n < 46; ++n) {
	cs+=config_buffer[n];
    }
    printf("new checksum: %x\n",cs);
    cfg_cs[0]=cs >> 8;
    cfg_cs[1]=(cs & 0xff);
    config_buffer[CONFIG_BUFFER_LENGTH-2]=cfg_cs[0];
    config_buffer[CONFIG_BUFFER_LENGTH-1]=cfg_cs[1];
    for (size_t n=0; n < CONFIG_BUFFER_LENGTH; ++n) {
	printf(" %02x",config_buffer[n]);
    }
    printf("\n");
//		  request_weather_message(handle,0,0xfffff);
//		  request_weather_message(handle,0,latest_haddr);
    request_set_config(handle,buffer,0);
    config_set=1;
  }
  else {
    request_weather_message(handle,0,latest_haddr);
  }
  first_sleep.tv_nsec=300000000;
  next_sleep.tv_nsec=10000000;
  config_requested=1;
}

void set_config(libusb_device_handle *handle)
{
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,config_buffer,CONFIG_BUFFER_LENGTH,1000);
  printf("***SETCONFIG*** %d\n",status);
  first_sleep.tv_nsec=85000000;
  next_sleep.tv_nsec=5000000;
}

void set_time(libusb_device_handle *handle,unsigned char *buffer)
{
  time_t t=time(NULL);
  struct tm *tm_result=localtime(&t);
  ++tm_result->tm_mon;
  tm_result->tm_year-=100;
  printf("20%02d-%02d-%02d %02d:%02d:%02d %d\n",tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec,tm_result->tm_wday);
  buffer[0]=0xd5;
  buffer[1]=0x0;
  buffer[2]=0xc;
  buffer[3]=((deviceID >> 8) & 0xff);
  buffer[4]=(deviceID & 0xff);
  buffer[5]=0xc0;
  cfg_cs[0]=buffer[7];
  cfg_cs[1]=buffer[8];
  buffer[6]=cfg_cs[0];
  buffer[7]=cfg_cs[1];
  int i=tm_result->tm_sec/10;
  buffer[8]=(i*16)+(tm_result->tm_sec % 10);
  i=tm_result->tm_min/10;
  buffer[9]=(i*16)+(tm_result->tm_min % 10);
  i=tm_result->tm_hour/10;
  buffer[10]=(i*16)+(tm_result->tm_hour % 10);
  buffer[11]=((tm_result->tm_mday % 10) << 4) | tm_result->tm_wday;
  i=tm_result->tm_mday/10;
  buffer[12]=((tm_result->tm_mon % 10) << 4) | ((i*16) >> 4);
  i=tm_result->tm_mon/10;
  buffer[13]=((tm_result->tm_year % 10) << 4) | ((i*16) >> 4);
  i=tm_result->tm_year/10;
  buffer[14]=((i*16) >> 4);
  for (size_t n=0; n < 15; ++n) {
    printf(" %02x",buffer[n]);
  }
  printf("\n");
  libusb_control_transfer(handle,0x21,0x9,0x3d5,0,buffer,15,1000);
  first_sleep.tv_nsec=85000000;
  next_sleep.tv_nsec=5000000;
}

typedef struct {
  float temp_out,dewp_out,wspd,wgust,rain_24hr,rain_1hr,barom;
  int rh_out,wdir;
} CurrentWeather;
CurrentWeather wx[2];
int cwx_idx=0;

void decode_current_wx(unsigned char *buffer,CurrentWeather *current_weather)
{
  current_weather->temp_out=(((buffer[42] & 0xf)*10000.+(buffer[43] >> 4)*1000.+(buffer[43] & 0xf)*100.+(buffer[44] >> 4)*10.+(buffer[44] & 0xf))/1000.-40.)*9./5.+32.;
  current_weather->dewp_out=(((buffer[78] & 0xf)*10000.+(buffer[79] >> 4)*1000.+(buffer[79] & 0xf)*100.+(buffer[80] >> 4)*10.+(buffer[80] & 0xf))/1000.-40.)*9./5.+32.;
  current_weather->rh_out=(buffer[106] >> 4)*10+(buffer[106] & 0xf);
  current_weather->wdir=lroundf((buffer[162] & 0xf)*22.5);
  if (current_weather->wdir == 0) {
    current_weather->wdir=360;
  }
  int idum=((buffer[172] >> 4) << 20) | ((buffer[172] & 0xf) << 16) | ((buffer[173] >> 4) << 12) | ((buffer[173] & 0xf) << 8) | ((buffer[174] >> 4) << 4) | (buffer[174] & 0xf);
  current_weather->wspd=((idum/256.)/100.)/1.60934;
  idum=((buffer[187] >> 4) << 20) | ((buffer[187] & 0xf) << 16) | ((buffer[188] >> 4) << 12) | ((buffer[188] & 0xf) << 8) | ((buffer[189] >> 4) << 4) | (buffer[189] & 0xf);
  current_weather->wgust=((idum/256.)/100.)/1.60934;
  current_weather->rain_1hr=((buffer[148] >> 4)*100000.+(buffer[148] & 0xf)*10000.+(buffer[149] >> 4)*1000.+(buffer[149] & 0xf)*100.+(buffer[150] >> 4)*10.+(buffer[150] & 0xf))/2540.;
  current_weather->rain_24hr=((buffer[137] >> 4)*100000.+(buffer[137] & 0xf)*10000.+(buffer[138] >> 4)*1000.+(buffer[138] & 0xf)*100.+(buffer[139] >> 4)*10.+(buffer[139] & 0xf))/2540.;
  current_weather->barom=((buffer[210] >> 4)*10000.+(buffer[210] & 0xf)*1000.+(buffer[211] >> 4)*100.+(buffer[211] & 0xf)*10.+(buffer[212] >> 4))/100.;
}

typedef struct {
  int this_addr,latest_addr;
  float temp_out,wspd,wgust,rain_raw,barom;
  int rh_out,wdir;
  char datetime[20];
} History;

void decode_history(unsigned char *buffer,History *history)
{
  for (size_t n=0; n < 30; ++n) {
    printf(" %02x",buffer[n]);
  }
  printf("\n");
  history->latest_addr=(buffer[6] << 8 | buffer[7]) << 8 | buffer[8];
latest_haddr=history->latest_addr-18;
  history->this_addr=(buffer[9] << 8 | buffer[10]) << 8 | buffer[11];
printf("latest address: %d  this address: %d\n",history->latest_addr,history->this_addr);
  history->temp_out=(((buffer[22] >> 4)*100.+(buffer[22] & 0xf)*10.+(buffer[23] >> 4))/10.-40.)*9./5.+32.;
  history->rh_out=(buffer[17] & 0xf)*10+(buffer[18] >> 4);
  history->wdir=lroundf((buffer[14] >> 4)*22.5);
  if (history->wdir == 0) {
    history->wdir=360;
  }
  int idum=(buffer[14] & 0xf) << 8 | buffer[15];
  history->wspd=idum/10.*2.237;
  idum=(buffer[12] & 0xf) << 8 | buffer[13];
  history->wgust=idum/10.*2.237;
  history->rain_raw=((buffer[16] >> 4)*100.+(buffer[16] & 0xf)*10.+(buffer[17] >> 4))/100.;
  history->barom=((buffer[19] & 0xf)*10000.+(buffer[20] >> 4)*1000.+(buffer[20] & 0xf)*100.+(buffer[21] >> 4)*10.+(buffer[21] & 0xf))/10.;
  sprintf(history->datetime,"20%02d-%02d-%02d %02d:%02d:00",(buffer[25] >> 4)*10+(buffer[25] & 0xf),(buffer[26] >> 4)*10+(buffer[26] & 0xf),(buffer[27] >> 4)*10+(buffer[27] & 0xf),(buffer[28] >> 4)*10+(buffer[28] & 0xf),(buffer[29] >> 4)*10+(buffer[29] & 0xf));
  history->datetime[19]='\0';
}

int wx_changed()
{
  int lwx_idx=1-cwx_idx;
  if (wx[cwx_idx].barom != wx[lwx_idx].barom) {
    return 1;
  }
  if (wx[cwx_idx].temp_out != wx[lwx_idx].temp_out) {
    return 1;
  }
  if (wx[cwx_idx].dewp_out != wx[lwx_idx].dewp_out) {
    return 1;
  }
  if (wx[cwx_idx].rh_out != wx[lwx_idx].rh_out) {
    return 1;
  }
  if (wx[cwx_idx].wdir != wx[lwx_idx].wdir) {
    return 1;
  }
  if (wx[cwx_idx].wspd != wx[lwx_idx].wspd) {
    return 1;
  }
  return 0;
}

void get_utc_date(time_t epoch,struct tm **tm_result)
{
  *tm_result=localtime(&epoch);
  (*tm_result)->tm_year+=1900;
  ++(*tm_result)->tm_mon;
  (*tm_result)->tm_hour+=7;
  if ((*tm_result)->tm_hour > 24) {
    (*tm_result)->tm_hour-=24;
    ++(*tm_result)->tm_mday;
    if ( (*tm_result)->tm_mday > mdays[(*tm_result)->tm_mon]) {
	++(*tm_result)->tm_mon;
	(*tm_result)->tm_mday=1;
	if ( (*tm_result)->tm_mon > 12) {
	  ++(*tm_result)->tm_year;
	  (*tm_result)->tm_mon=1;
	}
    }
  }
}

void request_get_config(libusb_device_handle *handle,unsigned char *buffer)
{
  buffer[0]=0xd5;
  buffer[1]=0x0;
  buffer[2]=0x9;
  buffer[3]=((deviceID >> 8) & 0xff);
  buffer[4]=(deviceID & 0xff);
  buffer[5]=0x3;
  buffer[6]=cfg_cs[0];
  buffer[7]=cfg_cs[1];
  buffer[8]=(comm_interval >> 4) & 0xff;
  int history_addr=0xffffff;
  buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
  buffer[10]=(history_addr >> 8) & 0xff;
  buffer[11]=(history_addr >> 0) & 0xff;
printf("request-get-config");
for (size_t n=0; n < 13; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,buffer,12,1000);
}

int timestamp(char *datetime)
{
  struct tm tm;
  strptime(datetime,"%Y-%m-%d %H:%M:%S",&tm);
  return mktime(&tm)-25200;
}

int now(time_t epoch)
{
  return mktime(localtime(&epoch))-25200;
}

void process_history_records(libusb_device_handle *handle)
{
  if (mysql_username != NULL && mysql_password != NULL) {
    open_mysql();
    time_t t1=time(NULL);
    const char *HISTORY_INSERT="insert into wx.history values(%d,%d,%d,%d,%d,%d,%d,%d,%d) on duplicate key update haddr = values(haddr)";
    char *ibuf=(char *)malloc(256*sizeof(char));
    unsigned char *data_buffer=(unsigned char *)malloc(273*sizeof(unsigned char));
    int status=mysql_query(&mysql,"select haddr from wx.history where timestamp in (select max(timestamp) from wx.history)");
    MYSQL_RES *result=mysql_use_result(&mysql);
    int addr;
    if (result != NULL) {
	MYSQL_ROW row;
	while (row=mysql_fetch_row(result)) {
	  addr=atoi(row[0]);
	}
    }
    else {
	addr=0xfffff;
    }
    mysql_free_result(result);
    size_t num_recs=0;
    while (1) {
	nanosleep(&first_sleep,NULL);
	while (1) {
	  if (get_state(handle) == 0x16) {
	    break;
	  }
	  nanosleep(&next_sleep,NULL);
	}
	int status=libusb_control_transfer(handle,0xa1,0x1,0x3d6,0,data_buffer,273,1000);
	printf("getframe: [%02x]",data_buffer[5]);
	for (size_t n=0; n < data_buffer[2]; ++n) {
	  printf(" %02x",data_buffer[n]);
	}
	printf("\n");
	if (cfg_cs[0] != 0x0 || cfg_cs[1] != 0x0 || data_buffer[5] == 0x40) {
	}
	else {
	  cfg_cs[0]=data_buffer[7];
	  cfg_cs[1]=data_buffer[8];
	}
	History history;
	if (data_buffer[5] == 0x80) {
	  decode_history(&data_buffer[3],&history);
	  printf("time %s\n",history.datetime);
	  printf("pressure: %.1fhPa\n",history.barom);
	  printf("outside temp: %.1fF\n",history.temp_out);
	  printf("outside RH: %d%%\n",history.rh_out);
	  printf("wind direction: %ddeg\n",history.wdir);
	  printf("wind speed: %.1fmph\n",history.wspd);
	  printf("wind gust: %.1fmph\n",history.wgust);
	  printf("rain: %.2fin\n",history.rain_raw);
	  for (size_t n=0; n < 256; ++n) {
	    ibuf[n]=0;
	  }
	  sprintf(ibuf,HISTORY_INSERT,timestamp(history.datetime),lround(history.barom*10.),lround(history.temp_out*10.),history.rh_out,history.wdir,lround(history.wspd*10.),lround(history.wgust*10.),lround(history.rain_raw*100.),history.this_addr);
	  mysql_query(&mysql,ibuf);
	  addr=history.this_addr;
	  ++num_recs;
	}
	request_weather_message(handle,0,addr);
	setTX(handle);
	if (history.this_addr == history.latest_addr) {
	  break;
	}
    }
    for (size_t n=0; n < 256; ++n) {
	ibuf[n]=0;
    }
    time_t t=time(NULL);
    struct tm *tm_t=localtime(&t);
    char midnight[20];
    sprintf(midnight,"20%02d-%02d-%02d 00:00:00",tm_t->tm_year-100,tm_t->tm_mon+1,tm_t->tm_mday);
    midnight[19]='\0';
    sprintf(ibuf,"select sum(i_rain)/100. from wx.history where timestamp > %d and timestamp <= %d",timestamp(midnight),now(t));
    mysql_query(&mysql,ibuf);
    result=mysql_use_result(&mysql);
    if (result != NULL) {
	MYSQL_ROW row;
	while (row=mysql_fetch_row(result)) {
	  rain_day=atof(row[0]);
	}
    }
printf("daily rain: %.2f\n",rain_day);
    time_t t2=time(NULL);
    printf("%d records processed in %d seconds\n",num_recs,(t2-t1));
    close_mysql();
    free(ibuf);
  }
}

void handle_frame()
{
}

char *trim(char *s)
{
  size_t len=strlen(s);
  size_t n=0;
  while (n < len && (s[n] == ' ' || s[n] == 0x9 || s[n] == 0xa || s[n] == 0xd)) {
    ++n;
  }
  int m=len-1;
  while (m > 0 && (s[m] == ' ' || s[m] == 0x9 || s[m] == 0xa || s[m] == 0xd)) {
    s[m--]='\0';
  }
  return &s[n];
}

void read_config()
{
  FILE *fp=fopen("/etc/jadewx/jadewx.conf","r");
  if (fp == NULL) {
    fprintf(stderr,"Unable to open /etc/jadewx/jadewx.conf\n");
    exit(1);
  }
  const size_t LINE_LENGTH=256;
  char line[LINE_LENGTH];
  while (fgets(line,LINE_LENGTH,fp) != NULL) {
    char *tline=trim(line);
    size_t tlen=strlen(tline);
    if (tlen > 0 && tline[0] != '#') {
	char section[LINE_LENGTH];
	if (tline[0] == '[' && tline[tlen-1] == ']') {
	  strncpy(&section[0],&tline[1],tlen-2);
	  section[tlen-2]='\0';
	}
	else {
	  size_t n=0;
	  while (tline[n] != '=' && n < tlen) {
	    ++n;
	  }
	  if (n != tlen) {
	    char name[LINE_LENGTH],value[LINE_LENGTH];
	    strncpy(&name[0],&tline[0],n);
	    name[n]='\0';
	    char *tname=trim(name);
	    strncpy(&value[0],&tline[n+1],tlen-n);
	    value[tlen-n]='\0';
	    char *tvalue=trim(value);
	    if (strcmp(tname,"username") == 0) {
		if (strcmp(section,"mysql") == 0) {
		  mysql_username=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(mysql_username,tvalue);
		}
	    }
	    else if (strcmp(tname,"password") == 0) {
		if (strcmp(section,"mysql") == 0) {
		  mysql_password=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(mysql_password,tvalue);
		}
		else if (strcmp(section,"Wunderground") == 0) {
		  WU_password=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(WU_password,tvalue);
		}
	    }
	    else if (strcmp(tname,"station") == 0) {
		if (strcmp(section,"Wunderground") == 0) {
		  WU_station=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(WU_station,tvalue);
		}
	    }
	  }
	}
    }
  }
  fclose(fp);
}

int main(int argc,char **argv)
{
  read_config();
  CURLcode ccode;
  if ( (ccode=curl_global_init(CURL_GLOBAL_ALL)) != 0) {
    fprintf(stderr,"unable to initialize CURL\n");
    exit(1);
  }
  CURL *curl=curl_easy_init();
  libusb_context *ctx;
  libusb_init(&ctx);
//  libusb_set_debug(ctx,LIBUSB_LOG_LEVEL_DEBUG);
  libusb_device **list;
  size_t num_devices;
  num_devices=libusb_get_device_list(ctx,&list);
printf("num devices: %d\n",num_devices);
  for (size_t n=0; n < num_devices; ++n) {
    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(list[n],&desc);
    if (desc.idVendor == 0x6666 && desc.idProduct == 0x5555) {
	libusb_device_handle *handle;
	int status;
	status=libusb_open(list[n],&handle);
	if (status != 0) {
	  printf("unable to open: %s\n",libusb_error_name(status));
	  exit(1);
	}
	status=libusb_detach_kernel_driver(handle,0);
	if (status != 0 && status != LIBUSB_ERROR_NOT_FOUND) {
	  printf("unable to detach kernal driver: %s\n",libusb_error_name(status));
	  exit(1);
	}
	status=libusb_claim_interface(handle,0);
	if (status != 0) {
	  printf("unable to claim interface: %s\n",libusb_error_name(status));
	  exit(1);
	}
printf("found! %d %d\n",n,status);
	status=libusb_control_transfer(handle,0x21,0xa,0,0,NULL,0,1000);
printf("setup status: %d\n",status);

	char serial_num[15];
	initialize_transceiver(handle,serial_num);
	do_setup(handle);

	printf("ready to pair...\n");
	char c;
	scanf("%c",&c);

	process_history_records(handle);

	unsigned char *data_buffer=(unsigned char *)malloc(512*sizeof(unsigned char));
	data_buffer[1]=0;
	const char *CURRENT_WX_INSERT="insert into wx.current values(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d) on duplicate key update rh = values(rh)";
	char *ibuf=(char *)malloc(256*sizeof(char));
	while (1) {
	  nanosleep(&first_sleep,NULL);
	  while (1) {
	    if (get_state(handle) == 0x16) {
		break;
	    }
	    nanosleep(&next_sleep,NULL);
	  }
	  status=libusb_control_transfer(handle,0xa1,0x1,0x3d6,0,data_buffer,273,1000);
	  printf("getframe: [%02x]",data_buffer[5]);
	  for (size_t n=0; n < data_buffer[2]; ++n) {
	    printf(" %02x",data_buffer[n]);
	  }
	  printf("\n");
	  if (cfg_cs[0] != 0x0 || cfg_cs[1] != 0x0 || data_buffer[5] == 0x40) {
	  }
	  else {
	    cfg_cs[0]=data_buffer[7];
	    cfg_cs[1]=data_buffer[8];
	  }
	  handle_frame();
if (data_buffer[6] >= 0x5f || data_buffer[5] == 0x20) {
	  switch (data_buffer[5]) {
	    case 0x20:
	    {
		setRX(handle);
		break;
	    }
	    case 0x40:
	    {
		store_config(handle,data_buffer);
		break;
	    }
	    case 0x60:
	    {
		cwx_idx=1-cwx_idx;
		decode_current_wx(&data_buffer[3],&wx[cwx_idx]);
		if (url == NULL) {
		  url=(char *)malloc(4096*sizeof(char));
		}
		for (size_t n=0; n < 256; ++n) {
		  ibuf[n]=0;
		}
//		printf("pressure: %.2fin\n",wx[cwx_idx].barom);
//		printf("outside temp: %.1fF\n",wx[cwx_idx].temp_out);
//		printf("outside dew point: %.1fF\n",wx[cwx_idx].dewp_out);
//		printf("outside RH: %d%%\n",wx[cwx_idx].rh_out);
//		printf("wind direction: %ddeg\n",wx[cwx_idx].wdir);
//		printf("wind speed: %.1fmph\n",wx[cwx_idx].wspd);
//		printf("wind gust: %.1fmph\n",wx[cwx_idx].wgust);
//		printf("1-hour rain: %.2fin\n",wx[cwx_idx].rain_1hr);
//		printf("24-hour rain: %.2fin\n",wx[cwx_idx].rain_24hr);
		if (WU_station != NULL && WU_password != NULL) {
		  time_t t=time(NULL);
		  struct tm *tm_result;
		  get_utc_date(t,&tm_result);
		  sprintf(url,URL_FORMAT,WU_station,WU_password,wx[cwx_idx].wdir,wx[cwx_idx].wspd,wx[cwx_idx].wgust,wx[cwx_idx].rh_out,wx[cwx_idx].dewp_out,wx[cwx_idx].temp_out,wx[cwx_idx].barom,wx[cwx_idx].rain_1hr,rain_day,tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec);
		  printf("%s\n",url);
		  curl_easy_setopt(curl,CURLOPT_URL,url);
		  curl_easy_perform(curl);
		}
		request_weather_message(handle,0,latest_haddr);
		first_sleep.tv_nsec=300000000;
		next_sleep.tv_nsec=10000000;
		break;
	    }
	    case 0x80:
	    {
		History history;
		decode_history(&data_buffer[3],&history);
		printf("time %s\n",history.datetime);
		printf("pressure: %.1fhPa\n",history.barom);
		printf("outside temp: %.1fF\n",history.temp_out);
		printf("outside RH: %d%%\n",history.rh_out);
		printf("wind direction: %ddeg\n",history.wdir);
		printf("wind speed: %.1fmph\n",history.wspd);
		printf("wind gust: %.1fmph\n",history.wgust);
		printf("rain: %.2fin\n",history.rain_raw);
		if (!config_requested) {
		  request_get_config(handle,data_buffer);
		}
		else {
		  sleep(2);
		  request_weather_message(handle,5,0xfffff);
		}
		first_sleep.tv_nsec=300000000;
		next_sleep.tv_nsec=10000000;
		break;
	    }
	    case 0xa1:
	    {
/*
// first config
	    data_buffer[0]=0xd5;
	    data_buffer[1]=0x0;
	    data_buffer[2]=0x9;
	    data_buffer[3]=0xf0;
	    data_buffer[4]=0xf0;
// ask for a config message
	    data_buffer[5]=0x3;
//	    data_buffer[6]=data_buffer[7];
//	    data_buffer[7]=data_buffer[8];
data_buffer[6]=0x0;
data_buffer[7]=0x1b;
	    data_buffer[8]=(comm_interval >> 4) & 0xff;
	    int history_addr=0xffffff;
	    data_buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
	    data_buffer[10]=(history_addr >> 8) & 0xff;
	    data_buffer[11]=(history_addr >> 0) & 0xff;
	    status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,data_buffer,273,1000);
printf("first config status %d\n",status);
*/
		first_sleep.tv_nsec=85000000;
		next_sleep.tv_nsec=5000000;
		break;
	    }
	    case 0xa2:
	    {
		set_config(handle);
		break;
	    }
	    case 0xa3:
	    {
		set_time(handle,data_buffer);
		break;
	    }
	    default:
	    {
		printf("***unhandled message type %x\n",(int)data_buffer[5]);
	    }
	  }
}
	  setTX(handle);
	}

	status=libusb_release_interface(handle,0);
	if (status != 0) {
	  printf("unable to release interface: %s\n",libusb_error_name(status));
	  exit(1);
	}
	status=libusb_attach_kernel_driver(handle,0);
	if (status != 0) {
	  printf("unable to attach kernal driver: %s\n",libusb_error_name(status));
	  exit(1);
	}
	libusb_close(handle);
    }
  }
  libusb_free_device_list(list,1);
  libusb_exit(ctx);
  curl_easy_cleanup(curl);
}