#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "uart.h"

#include "ssd1306.h"
#include "user_interface.h"
#include "ieee80211_structs.h"
#include "sdk_structs.h"

//#define DEBUG
#define SNIFFER_ARRAY_SIZE 200
#define SNIFF_TIME_PER_CHANNEL 120000 //ms
// 60000 gives nice bar graph of stations

#define WIFI_MODE_STATION 0x01
#define WIFI_MODE_SOFTAP 0x02
#define WIFI_MODE_STATION_AND_SOFTAP 0x03

uint8_t scan_passes_required = 20;
uint8_t scan_pass_current = 0;
uint8_t known_bssids[100][6];
uint16_t number_aps = 0;
uint8_t channels[14];
uint8_t channels_ap[14];
uint8_t channels_sta[14];
uint8_t sniffed_macs[SNIFFER_ARRAY_SIZE][8] __attribute__((aligned(4)));
//uint8_t sniffed_macs[100][8] __attribute__((aligned(4)));
uint16_t sniffed_macs_number = 0;
uint8_t sniffed_bssids[SNIFFER_ARRAY_SIZE][8] __attribute__((aligned(4)));
//uint8_t sniffed_bssids[100][8] __attribute__((aligned(4)));
uint16_t sniffed_bssids_number = 0;
uint8_t sniff_channel = 1;
uint32_t sniffed_packets = 0;

#define TEST_QUEUE_LEN 4

static volatile os_timer_t some_timer;
void ICACHE_FLASH_ATTR scan_done(void *arg, STATUS status);
void wifi_sniffer_packet_handler(uint8_t *buff, uint16_t len);
//void wifi_sniffer_packet_handler_v2(uint8_t *buff, uint16_t len);
char* itoa(uint32_t i, char *buf);
os_event_t scanQueue[TEST_QUEUE_LEN];
void ICACHE_FLASH_ATTR some_timerfunc(void *arg);

uint16_t number_of_stations(void)
{
  // walk through sniffed_macs[] elements and count only if 
  // the current element is not in sniffed_bssids[]
  uint16_t number = 0;
  for (uint32_t i=0; i<sniffed_macs_number; i++)
  {
    for (uint32_t j=0; j<sniffed_bssids_number; j++)
    {
      if (!(os_memcmp(sniffed_macs[i],sniffed_bssids[j], 6))) 
      {
        // MACs match
        break;
      }
      // MAC did not match a BSSID
      number++;
    }
  }
}

void ICACHE_FLASH_ATTR show_results(void)
{
  static uint8_t state;

  ssd1306_clear();
  
  if (!state) 
  {
    draw_channel_graph(0,31,channels_ap);
    ssd1306_text(WIDTH-(2*8),HEIGHT-1-8,"AP", 0);
    state = 1;
  }else if (state == 1)
  {
    draw_channel_graph(0,31,channels_sta);
    ssd1306_text(WIDTH-(2*8),HEIGHT-1,"ST", 0);
    state = 2;
  }else if (state == 2)
  {
    draw_channel_graph(0,31,channels);
    ssd1306_text(WIDTH-(2*8),HEIGHT-1,"ST", 0);
    ssd1306_text(WIDTH-(2*8),HEIGHT-1-8,"AP", 0);
    state = 0;
  }
  ssd1306_commit();
}
void ICACHE_FLASH_ATTR end_promiscuous_scan(void *arg)
{
  wifi_promiscuous_enable(0);
  // filter APs out from sniffed_macs to get the number of stations
  uint16_t num_sta = number_of_stations();
  uint16_t dev = num_sta + sniffed_bssids_number;

  channels_sta[sniff_channel] = num_sta;
  channels_ap[sniff_channel] = sniffed_bssids_number;
  channels[sniff_channel] = num_sta + sniffed_bssids_number;
  // output results, number of APs and stations
  char n[21];
  char output[100];
  ssd1306_clear();
  os_sprintf(output, "Channel %d Done!", sniff_channel);
  ssd1306_text(0, 7, output, 0);
  os_sprintf(output, "Sniffed %d Pks", sniffed_packets);
  ssd1306_text(0, 15, output, 0);
  os_sprintf(output, "%d APs %d STAs", sniffed_bssids_number, num_sta);
  ssd1306_text(0, 23, output, 0);
  ssd1306_commit();

  /*
  char n[21];
  char output[100];
  os_sprintf(output, "found %d devices", sniffed_macs_number);
  ssd1306_text(0, 7, output);
  ssd1306_commit();
  */

  //channels[sniff_channel] = sniffed_macs_number;
  //os_printf("number of MACs caught: %d\n", sniffed_macs_number);

  sniff_channel++;
  if (sniff_channel <= 14)
  {
    // start a new scan in a hackish way
    os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
    os_timer_arm(&some_timer, 500, 0);
  }else{
    os_timer_setfn(&some_timer, (os_timer_func_t *)show_results, NULL);
    os_timer_arm(&some_timer, 3000, 1);
  }
}


void ICACHE_FLASH_ATTR some_timerfunc(void *arg)
{
#ifdef DEBUG
  os_printf("configuring prom mode\n");
#endif
  
  os_bzero(sniffed_macs, SNIFFER_ARRAY_SIZE*8);
  sniffed_macs_number = 0;
  os_bzero(sniffed_bssids, SNIFFER_ARRAY_SIZE*8);
  sniffed_bssids_number = 0;
  sniffed_packets = 0;

  wifi_set_channel(sniff_channel);

  // Wifi setup
  wifi_set_opmode(WIFI_MODE_STATION);
  wifi_promiscuous_enable(0);
  wifi_station_disconnect();

  // Set sniffer callback
  wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler);
  // performance test: v2() is not capturing more packets than our handler
  // so don't worry about performance of wifi_sniffer_packet_handler()
  //wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler_v2);

  if (sniff_channel == 0) ssd1306_clear();
  char output[100];
  os_sprintf(output, "Sniffing Ch %d..", sniff_channel);
  ssd1306_text(0, 31, output, 0);
  ssd1306_commit();

  os_timer_setfn(&some_timer, (os_timer_func_t *)end_promiscuous_scan, NULL);
  //os_timer_arm(&some_timer, 60000, 0);
  os_timer_arm(&some_timer, SNIFF_TIME_PER_CHANNEL, 0);

  wifi_promiscuous_enable(1);
}

void draw_vert_line_inverted(uint8_t x)
{
  for (uint8_t i=0; i < HEIGHT; i++)
  {
    ssd1306_pixel(x, i, 0, 0);
  }
}

void draw_bar(uint8_t x, uint8_t y, uint8_t val)
{
  uint8_t overflow = 0;
  if (val > 32) 
  {
    val = 32;
    overflow = 1;
  }
  // draw 1 line
  for (uint8_t v = 0; v < val; v++)
  {
    for (uint8_t i=0; i< 7; i++)
    {
      ssd1306_pixel(x+i, y-(v), 1, 0);
    }
  }
  if (overflow)
  {
    // draw an inverted '^' on top of the bar
    // to signify that the value is even higher then the
    // bargraph shows
    ssd1306_char(x, 7, '^', 1);
    // now cut off the right most character column of the inverted background
    draw_vert_line_inverted(x+7);
  }
}

void draw_channel_graph(uint8_t x, uint8_t y, uint8_t *ch)
{
  // check if we need to ratio down the number of devices for displaying
  uint8_t overflowing_channels = 0;
  uint8_t ratio = 0;
  for (uint8_t i=1; i <= 14; i++)
  {
    if (ch[i] > 32) overflowing_channels++;
  }
  if (overflowing_channels > 3)
  {
    ratio = 1;
    // draw a 'x2' mark in the top right corner 
    // to signify that actual values are x2 the 
    // displayed ones
    ssd1306_text(WIDTH-(2*8),7,"x2", 0);
  }
  for (uint8_t i=1; i <= 14; i++)
  {
    draw_bar(x+(i-1)*8,y, ratio==0 ? ch[i] : ch[i] / 2);
  }

}

char* itoa(uint32_t i, char *buf)
{
    char tmp[21];
    int8_t k = 0;
    tmp[20] = 0;

    do 
    {
        // leave nullbyte  in tmp[20] 
        tmp[19 - k] = i % 10 | 0x30;
        i /= 10;
        k++;
    } while (i);
    os_memcpy(buf, tmp+20-k, k+1);
    
    return buf;
}

void ICACHE_FLASH_ATTR scan_task(void *arg)
{
  scan_pass_current++;
  if (scan_pass_current <= scan_passes_required) 
  {
    wifi_scan_type_t scan_type;
    if (scan_pass_current == 1)
    {
      scan_type = WIFI_SCAN_TYPE_ACTIVE;
    }else
    {
      scan_type = WIFI_SCAN_TYPE_PASSIVE;
    }
    struct scan_config scan_conf = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = 1,
      .scan_type = scan_type,
    };
    os_printf("Scanning...\n");
    ssd1306_text(0,7,"Scanning...", 0);
    ssd1306_text(0,15,"Pass ", 0);
    char pass[21];
    itoa(scan_pass_current, pass);
    ssd1306_text(5*8,15,pass, 0);
    ssd1306_commit();
    wifi_station_scan(&scan_conf, scan_done);
  }else
  {
    // all the passes completed, print results
    char nap[21];
    itoa(number_aps, nap);
    /*
    ssd1306_text(0,7,"found ");
    ssd1306_text(6*8,7,nap);
    ssd1306_text((6+os_strlen(nap))*8,7," APs");
    */
    ssd1306_text(111,31,nap, 0);
    uint8_t pos = 0;
    char output[100];
    for (uint8_t ch = 1; ch <= 14; ch++)
    {
      itoa(channels[ch], nap);
      uint8_t len = strlen(nap);
      nap[len] = ' ';
      nap[len+1] = '\0';
      os_memcpy(output+pos, nap, os_strlen(nap));
      pos += os_strlen(nap);
    }
    output[pos] = '\0';
    //ssd1306_text(0,15,output);
    //ssd1306_commit();
    os_printf("%s", output);
    ssd1306_text(0,7,"                ", 0);
    ssd1306_text(0,15,"                ", 0);
    draw_channel_graph(0,31,channels);
    ssd1306_commit();

    // reset pass counter
    scan_pass_current = 0;
  }
}

uint8_t record_ap(struct bss_info *ap)
{
  //os_printf("comparing...\n");
  for (uint16_t i=0; i<number_aps; i++)
  {
    // compare BSSID
    uint8_t flag_match = 1;
    for (uint8_t byte=0; byte<6; byte++)
    {
      if (known_bssids[i][byte] != ap->bssid[byte])
      {
        flag_match = 0;
        break;
      }
    }
    // BSSIDs match
    
    if (flag_match)
    {
      //os_printf("MATCH AND RETURN 0\n");
      return 0;
    }
    system_soft_wdt_feed();
  }

  os_printf("NEW AP found\n");
  os_memcpy(&known_bssids[number_aps], ap->bssid, 6);
  channels[ap->channel]++;
  //known_bssids[number_aps++] = bssid;
  number_aps++;
  return 1;
}

void ICACHE_FLASH_ATTR scan_done(void *arg, STATUS status)
{
  //os_printf("scan done\r\n");
  struct bss_info *res = (struct bss_info*) arg;

  uint16_t pass_aps = 0;
  while (res != NULL)
  {
    record_ap(res);
    res = STAILQ_NEXT(res, next);
    pass_aps++;
  }
  os_printf("found %d APs\n", pass_aps);
  if (scan_pass_current <= scan_passes_required) 
  {
    system_os_post(USER_TASK_PRIO_0, 0, 0 );
  }
}

/*
// performance test function
void wifi_sniffer_packet_handler_v2(uint8_t *buff, uint16_t len)
{
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
  // Second layer: define pointer to where the actual 802.11 packet is within the structure
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  // Third layer: define pointers to the 802.11 packet header and payload
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
  // Pointer to the frame control section within the packet header
  const wifi_header_frame_control_t *frame_ctrl = (wifi_header_frame_control_t *)&hdr->frame_ctrl;

  if ((frame_ctrl->type == WIFI_PKT_MGMT && frame_ctrl->subtype == BEACON) ||
      (frame_ctrl->type == WIFI_PKT_DATA))
  {
    sniffed_packets++;
  }
}
*/

// callback function on received packet
// keep as short as possible and parse later
void wifi_sniffer_packet_handler(uint8_t *buff, uint16_t len)
{
// First layer: type cast the received buffer into our generic SDK structure
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
  // Second layer: define pointer to where the actual 802.11 packet is within the structure
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  // Third layer: define pointers to the 802.11 packet header and payload
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  // we are not using this:
  //const uint8_t *data = ipkt->payload;

  // Pointer to the frame control section within the packet header
  const wifi_header_frame_control_t *frame_ctrl = (wifi_header_frame_control_t *)&hdr->frame_ctrl;

/*
  uint8_t addr1_new = 1;
  uint8_t addr2_new = 1;
*/

  /* don't think this has any effect, channel seems to be just our configured
   * listening channel but can be adjacent channel in promiscuous mode
  if (ppkt->rx_ctrl.channel != sniff_channel)
  {
    return;
  }
  */

  if (frame_ctrl->type == WIFI_PKT_MGMT && frame_ctrl->subtype == BEACON)
  {
    sniffed_packets++;
    for (uint8_t i=0; i<sniffed_bssids_number; i++)
    {
      if (*(uint16_t*)(&sniffed_bssids[i][4]) == *(uint16_t*)(hdr->addr2+4) &&
          *(uint16_t*)(&sniffed_bssids[i][2]) == *(uint16_t*)(hdr->addr2+2) &&
          *(uint16_t*)(&sniffed_bssids[i][0]) == *(uint16_t*)(hdr->addr2+0))
        {
          // matching MACS
#ifdef DEBUG
          //os_printf("SEEN THIS MAC BEFORE\n");
#endif
          //addr2_new = 0;

          // if we land here, we are done with parsing this packet
          return;
        }
    }
/*
    if (addr2_new)
    {
*/
      os_memcpy(&sniffed_bssids[sniffed_bssids_number], hdr->addr2, 6);
      sniffed_bssids_number++;
#ifdef DEBUG
      os_printf(" SA (AP): ");
      os_printf(MACSTR, MAC2STR(hdr->addr2));
      os_printf("\n");
#endif
//    }
    return;
  }

  if (frame_ctrl->type == WIFI_PKT_DATA)
  {
    sniffed_packets++;
    // BE VERY CAREFUL MATCHING THIS WAY, BYTE ORDER CHANGES WHEN CASTING uint8 to uint16/32
    // (but it works as long as the bytes are the same)
    if (! (
          // match broadcast
          ((*(uint16_t*)(hdr->addr1+4) == 0xFFFF && *(uint32_t*)hdr->addr1 == 0xFFFFFFFF))
          ||
          // match ipv6 multicast 33:33
          ((*(uint16_t*)hdr->addr1 == 0x3333))
          || 
          // match 01:00:5e:00 - 01:00:5e:7f ipv4 multicast
          // compare byte for byte because order changes
          // can be optimized if we use correct uint16/32 comparison value
          (hdr->addr1[0] == 0x01 && hdr->addr1[1] == 0x00 && hdr->addr1[2] == 0x5e && hdr->addr1[3] >> 7 == 0)
          )
       )
    {
      for (uint8_t i=0; i<sniffed_macs_number; i++)
      {
        
        if (*(uint16_t*)(&sniffed_macs[i][4]) == *(uint16_t*)(hdr->addr1+4) &&
           (*(uint32_t*)(&sniffed_macs[i][0]) == *(uint32_t*)hdr->addr1))
          {
            // matching MACS
  #ifdef DEBUG
            //os_printf("SEEN THIS MAC BEFORE\n");
  #endif
            //addr1_new = 0;
            //break;
            goto no_match;
          }
      }
/*
      if (addr1_new)
      {
*/
        os_memcpy(&sniffed_macs[sniffed_macs_number], hdr->addr1, 6);
        sniffed_macs_number++;
#ifdef DEBUG
        os_printf(" RA: ");
        os_printf(MACSTR, MAC2STR(hdr->addr1));
        os_printf("\n");
#endif
      //}
    }

no_match:
    if (! (
          // can be optimized to use one uint16 and one uint32 comparison
          ((*(uint16_t*)(hdr->addr2+4) == 0xFFFF && *(uint16_t*)(hdr->addr2+2) == 0xFFFF
              && *(uint16_t*)hdr->addr2 == 0xFFFF))
          ||
          (*(uint16_t*)hdr->addr2 == 0x3333)
          ||
          // can be optimized as corresponding one above
          (hdr->addr2[0] == 0x01 && hdr->addr2[1] == 0x00 && hdr->addr2[2] == 0x5e && hdr->addr2[3] >> 7 == 0)
          )
       )
    {
      for (uint8_t i=0; i<sniffed_macs_number; i++)
      {
        // can not be optimized further, because alignment of addr2 is different than
        // alignment of addr1
        if (*(uint16_t*)(&sniffed_macs[i][4]) == *(uint16_t*)(hdr->addr2+4) &&
            *(uint16_t*)(&sniffed_macs[i][2]) == *(uint16_t*)(hdr->addr2+2) &&
            *(uint16_t*)(&sniffed_macs[i][0]) == *(uint16_t*)(hdr->addr2+0))
          {
            // matching MACS
  #ifdef DEBUG
            //os_printf("SEEN THIS MAC BEFORE\n");
  #endif
       //     addr2_new = 0;
            return;
            //break;
          }
      }
/*
      if (addr2_new)
      {
*/
        os_memcpy(&sniffed_macs[sniffed_macs_number], hdr->addr2, 6);
        sniffed_macs_number++;
#ifdef DEBUG
        os_printf(" SA: ");
        os_printf(MACSTR, MAC2STR(hdr->addr2));
        os_printf("\n");
#endif
      //}
    }
  }
}

void ICACHE_FLASH_ATTR user_init()
{

  // enable serial debugging
  //uart_div_modify(0, UART_CLK_FREQ / 115200);
#ifdef DEBUG
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
#endif
  
  ssd1306_init();

  /*
  uint8_t str[100] = {'H','i',' ','J','e','n','n','y',' ',3,3,3,'\0'};
  
  for (uint8_t k = 0; k < 100; k++)
  {
    str[k] = k+1;
  }
  str[60] = '\0';

  ssd1306_text(0,7,str);
  //ssd1306_pixel(0,0,1,0);
  ssd1306_commit();
  */

/*
  // set up promiscuous mode
  wifi_set_opmode(WIFI_MODE_STATION);
  wifi_station_disconnect();
  wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler);
  wifi_set_channel(1);
  wifi_promiscuous_enable(1);
*/
  


  //system_os_task(scan_task,USER_TASK_PRIO_0,scanQueue,TEST_QUEUE_LEN);
  //system_os_post(USER_TASK_PRIO_0, 0, 0 );

  // setup timer (500ms, repeating)
  os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
  os_timer_arm(&some_timer, 3000, 0);
}



