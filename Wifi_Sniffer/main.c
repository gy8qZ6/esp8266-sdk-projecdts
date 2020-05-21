#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "mem.h"

#include "uart.h"

#include "ssd1306.h"
#include "user_interface.h"
#include "ieee80211_structs.h"
#include "sdk_structs.h"

//#define DEBUG
//#define SNIFFER_ARRAY_SIZE 200
#define SNIFF_TIME_PER_CHANNEL 300 //ms
// 60000 gives nice bar graph of stations

#define WIFI_MODE_STATION 0x01
#define WIFI_MODE_SOFTAP 0x02
#define WIFI_MODE_STATION_AND_SOFTAP 0x03

// wifi packet
#define TAGGED_PARAMS_START    36
#define TAGGED_PARAMS_SSID_ID  0
#define TAGGED_PARAMS_CH_ID    3

#define MALLOC_PIECE  10
// call realloc on malloc'ed array 
#define EXTEND_SPACE_IF_NEEDED(array,len,type)                            \
        do {                                                              \
          if (len % MALLOC_PIECE == 0)                                    \
          {                                                               \
            array = os_realloc(array, (len + MALLOC_PIECE)*sizeof(type)); \
          }                                                               \
        } while(0);
/*
 * sniffing:
 * - sniff packets on each channel
 * - put packet data into:
 *    - struct pk_beacon { bssid, ch, ssid }[15]
 *    - struct pk_data { SA, RA }[15]
 * - after sniffing, assign a BSSID to pk_data-addresses that are not
 *   bssids, and thus assign them a ch
 * - now we have clean lists of APs and STAs with their correct channel
 *
 * TODO:
 * - implement looking for matching bssids in neighboring channels
 *   when unable to match a data_addr_pair
 * - log number of remaining data_addr_pairs in every round for each channel
 */

// wifi access point
typedef struct {
  uint8_t bssid[6];
  uint8_t ssid[33];
  uint8_t channel;
} access_point;
// wifi station
typedef struct {
  uint8_t mac[6];
  uint8_t associated_bssid[6];
  uint8_t channel;
} station;
// save data packages sender and rcv address 
// to associate station to access point
typedef struct {
  uint8_t SA[6];
  uint8_t RA[6];
} data_addr_pair;

// put sniffed APs (beacons) and address pairs (data pkts)
// into these lists while sniffing the channels 1-14
access_point* sniffed_aps[15] = {0};
data_addr_pair* sniffed_macs[15] = {0};
uint8_t sniffed_aps_len[15] = {0};
uint16_t sniffed_macs_len[15] = {0};
// populate this list after sniffing all channels
// by using the two lists above
// basically determine channel and ssid of all stations
station* sniffed_stations[15] = {0};
uint8_t sniffed_stations_len[15] = {0};

//uint8_t known_bssids[100][6];
//uint16_t number_aps = 0;
uint8_t channels[15];
//uint8_t channels_ap[15];
//uint8_t channels_sta[15];
//uint8_t sniffed_macs[SNIFFER_ARRAY_SIZE][8] __attribute__((aligned(4)));
//uint8_t sniffed_macs[100][8] __attribute__((aligned(4)));
//uint16_t sniffed_macs_number = 0;
//uint8_t sniffed_bssids[SNIFFER_ARRAY_SIZE][8] __attribute__((aligned(4)));
//uint8_t sniffed_bssids[100][8] __attribute__((aligned(4)));
//uint16_t sniffed_bssids_number = 0;
uint8_t sniff_channel = 1;
uint32_t sniffed_packets = 0;

#define TEST_QUEUE_LEN 4

static volatile os_timer_t some_timer;
os_event_t scanQueue[TEST_QUEUE_LEN];
void wifi_sniffer_packet_handler(uint8_t *buff, uint16_t len);
//void wifi_sniffer_packet_handler_v2(uint8_t *buff, uint16_t len);
//void ICACHE_FLASH_ATTR wifi_sniffer_packet_handler_v3(uint8_t *buff, uint16_t len);
void ICACHE_FLASH_ATTR scan_prepare(void *arg);       // calls scan_begin()
void ICACHE_FLASH_ATTR scan_begin(void *arg);         // calls scan_hop_channel()
void scan_hop_channel(void *arg);   // loops and calls show_results()
void ICACHE_FLASH_ATTR show_results(void);            // updates display and calls scan_begin()

void ICACHE_FLASH_ATTR show_results(void)
{
  static uint8_t state;

  ssd1306_clear();
  
  if (!state) 
  {
    draw_channel_graph(0,31,sniffed_aps_len);
    ssd1306_text(WIDTH-(2*8),HEIGHT-1-8,"AP", 0);
    state = 1;
  }else if (state == 1)
  {
    draw_channel_graph(0,31,sniffed_stations_len);
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

  /*
  // start new scan
  os_timer_disarm(&some_timer);
  os_timer_setfn(&some_timer, (os_timer_func_t *)scan_begin, NULL);
  os_timer_arm(&some_timer, 100, 0);
  */
  scan_begin(NULL);
}


void ICACHE_FLASH_ATTR process_results(void)
{

/*
  for (uint8_t i=1; i<15; i++)
  {
    sniffed_stations[i] = os_malloc(MALLOC_PIECE * sizeof(station));
  }
*/
  
  // populate sniffed_stations[15] by using the data in
  // sniffed_aps[15] and sniffed_macs[15]
  // i.e. determine the stations channel by associating
  // it with its AP from which we know the channel
  for (uint8_t i=1; i<15; i++)
  {
    // new sniffed_macs[i] list and len var
    // for data_addr_pairs that can't be matched to a BSSID
    // in this round, so keep that data for the following round
    data_addr_pair* tmp_sniffed_macs = NULL;
    uint16_t tmp_sniffed_macs_len = 0;
    for (uint16_t j=0; j<sniffed_macs_len[i]; j++)
    {
      // look for bssid in this data_addr_pair
      data_addr_pair *dap = sniffed_macs[i] + j;
      uint8_t identified = 0;
      for (uint16_t k=0; k<sniffed_aps_len[i]; k++)
      {
        uint8_t *bssid = NULL;
        uint8_t *sta_mac = NULL;
        if (!(os_memcmp(sniffed_aps[i][k].bssid, dap->SA, 6)))
        {
          // dap.SA matches AP
          bssid = dap->SA;
          sta_mac = dap->RA;
        } else if (!(os_memcmp(sniffed_aps[i][k].bssid, dap->RA, 6)))
        {
          // dap.RA matches AP
          bssid = dap->RA;
          sta_mac = dap->SA;
        }
         
        if (bssid != NULL) 
        {
          identified = 1;
          // TODO remove dap from sniffed_macs, i.e. create new sniffed_macs for unmatched elements
          // make sure we have enough memory
          EXTEND_SPACE_IF_NEEDED(sniffed_stations[i],sniffed_stations_len[i],station);
          /*
          if (sniffed_stations_len[i] % MALLOC_PIECE == 0)
          {
            // realloc to make room for new data_addr_pair struct
            sniffed_stations[i] = os_realloc(sniffed_stations[i], 
                    sniffed_stations_len[i]*sizeof(station) + 
                    MALLOC_PIECE * sizeof(station));
          }
          */

          // record the station
          station *sta = sniffed_stations[i] + sniffed_stations_len[i];
          os_memcpy(sta->mac, sta_mac, 6);
          os_memcpy(sta->associated_bssid, bssid, 6);
          sta->channel = i;
          sniffed_stations_len[i]++;
          break;
        }
      }
      if (!identified)
      {
        // add unidentified station to new list
        EXTEND_SPACE_IF_NEEDED(tmp_sniffed_macs,tmp_sniffed_macs_len,data_addr_pair);
        /*
        if (tmp_sniffed_macs_len % MALLOC_PIECE == 0)
        {
          // realloc to make room for new data_addr_pair struct
          tmp_sniffed_macs = os_realloc(tmp_sniffed_macs, 
                  tmp_sniffed_macs_len*sizeof(data_addr_pair) + 
                  MALLOC_PIECE * sizeof(data_addr_pair));
        }
        */
        // record the addr pair
        data_addr_pair *dap_tmp = tmp_sniffed_macs + tmp_sniffed_macs_len;
        os_memcpy(dap_tmp->SA, dap->SA, 6);
        os_memcpy(dap_tmp->RA, dap->RA, 6);
        tmp_sniffed_macs_len++;
      }
    }
    // if new list is not empty, replace sniffed_macs[i] with it
    // and set sniffed_macs_len[i] accordingly
    // but first os_free(sniffed_macs[i])
    os_free(sniffed_macs[i]);
    sniffed_macs[i] = tmp_sniffed_macs;
    sniffed_macs_len[i] = tmp_sniffed_macs_len;
    system_soft_wdt_feed();
  }
  for (uint8_t i=1; i<15; i++)
  {
    channels[i] = sniffed_aps_len[i] + sniffed_stations_len[i];
  }
}

/*
void ICACHE_FLASH_ATTR end_sniffing(void *arg)
{
  wifi_promiscuous_enable(0);

  // output results, number of APs and stations
  /*
  char n[21];
  char output[100];
  ssd1306_clear();
  os_sprintf(output, "Channel %d Done!", sniff_channel);
  ssd1306_text(0, 7, output, 0);
  os_sprintf(output, "Sniffed %d Pks", sniffed_packets);
  ssd1306_text(0, 15, output, 0);
  os_sprintf(output, "%d APs %d STAs", sniffed_aps_len[sniff_channel], 0);
  ssd1306_text(0, 23, output, 0);
  ssd1306_commit();
  */

  /*
  char n[21];
  char output[100];
  os_sprintf(output, "found %d devices", sniffed_macs_number);
  ssd1306_text(0, 7, output);
  ssd1306_commit();
  */

  //channels[sniff_channel] = sniffed_macs_number;
  //os_printf("number of MACs caught: %d\n", sniffed_macs_number);
/*
  sniff_channel++;
  os_timer_disarm(&some_timer);
  if (sniff_channel <= 14)
  {
    // start a new scan in a hackish way
    os_timer_setfn(&some_timer, (os_timer_func_t *)scan_start, NULL);
    os_timer_arm(&some_timer, 500, 0);
  }else{
    process_results();
    os_timer_setfn(&some_timer, (os_timer_func_t *)show_results, NULL);
    os_timer_arm(&some_timer, 3000, 0);
  }
}
*/

void scan_hop_channel(void *arg)
{
  wifi_promiscuous_enable(0);
  
  if (sniff_channel < 14)
  {
    wifi_set_channel(++sniff_channel);
    wifi_promiscuous_enable(1);
  }else
  {
    process_results();
    show_results();
    /*
    os_timer_disarm(&some_timer);
    os_timer_setfn(&some_timer, (os_timer_func_t *)show_results, NULL);
    //os_timer_arm(&some_timer, 60000, 0);
    os_timer_arm(&some_timer, 100, 0);
    */
    
  }
}

void ICACHE_FLASH_ATTR scan_begin(void *arg)
{
  sniff_channel = 1;
  wifi_set_channel(sniff_channel);
  sniffed_packets = 0;

  os_timer_disarm(&some_timer);
  os_timer_setfn(&some_timer, (os_timer_func_t *)scan_hop_channel, NULL);
  //os_timer_arm(&some_timer, 60000, 0);
  os_timer_arm(&some_timer, SNIFF_TIME_PER_CHANNEL, 1);

  wifi_promiscuous_enable(1);
}

void ICACHE_FLASH_ATTR scan_prepare(void *arg)
{
#ifdef DEBUG
  os_printf("configuring prom mode\n");
#endif
  
  /*
  os_bzero(sniffed_macs, SNIFFER_ARRAY_SIZE*8);
  sniffed_macs_number = 0;
  os_bzero(sniffed_bssids, SNIFFER_ARRAY_SIZE*8);
  sniffed_bssids_number = 0;
  */

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
  //wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler_v3);

  /*
  if (sniff_channel == 1) ssd1306_clear();
  //char output[100];
  //os_sprintf(output, "Sniffing Ch %d..", sniff_channel);
  //ssd1306_text(0, 31, output, 0);
  ssd1306_commit();
  */

  /*
  os_timer_disarm(&some_timer);
  os_timer_setfn(&some_timer, (os_timer_func_t *)scan_begin, NULL);
  //os_timer_arm(&some_timer, 60000, 0);

  // TODO don;t use timer, just start immediately
  os_timer_arm(&some_timer, 100, 0);
  */
  scan_begin(NULL);
}

void ICACHE_FLASH_ATTR draw_vert_line_inverted(uint8_t x)
{
  for (uint8_t i=0; i < HEIGHT; i++)
  {
    ssd1306_pixel(x, i, 0, 0);
  }
}

void ICACHE_FLASH_ATTR draw_bar(uint8_t x, uint8_t y, uint8_t val)
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

void ICACHE_FLASH_ATTR draw_channel_graph(uint8_t x, uint8_t y, uint8_t *ch)
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
/*
void ICACHE_FLASH_ATTR wifi_sniffer_packet_handler_v3(uint8_t *buff, uint16_t len)
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


  if (frame_ctrl->type == WIFI_PKT_MGMT && frame_ctrl->subtype == BEACON)
  {
    const wifi_pkt_mgmt_t *beacon = (wifi_pkt_mgmt_t*) buff; 
    uint8_t * const p = &(beacon->buf);
    // TODO do this more robust: recognize ssid field by parameter number
    // TODO restore deleted functions because I might need them still
    uint8_t len = 0;
    uint8_t ssid[33] = {0};
    uint8_t ch = 0;
    for (uint16_t i = TAGGED_PARAMS_START; i < 112; i+= 2 + p[i+1])
    {
      if (p[i] == TAGGED_PARAMS_SSID_ID)
      {
        // parse SSID
        uint8_t ssid_len = p[i+1];
        os_memcpy(ssid, &p[i+2], ssid_len);
        ssid[ssid_len] = '\0';
      } else if (p[i] == TAGGED_PARAMS_CH_ID)
      {
        // parse CH
        ch = p[i+2];
      }

      // if SSID and CH set, break out
      if (ch && ssid[0])
      {
        break;
      }
    }

    uint8_t ch_str[20];
    //os_sprintf(ch_str, "channel: %d", ch);
    //ssd1306_text(0, 7, "                ", 0);
    //ssd1306_text(0, 15, "                ", 0);
    //ssd1306_text(0, 23, "                ", 0);
    ssd1306_text(0, 7, ssid, 0);
    //ssd1306_text(0, 23, ch_str, 0);
    ssd1306_commit();
    //os_delay_us(65535);
    //os_printf("beacon SSID: %s\n", ssid);
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

  if (frame_ctrl->type == WIFI_PKT_MGMT && frame_ctrl->subtype == BEACON)
  {
    sniffed_packets++;

    // determine channel so we can look in corresponding list
    // of sniffed APs if we have already recorded this AP
    const wifi_pkt_mgmt_t *beacon = (wifi_pkt_mgmt_t*) buff; 
    uint8_t * const p = &(beacon->buf);
    uint8_t ch = 0;

    for (uint16_t i = TAGGED_PARAMS_START; i < 112; i+= 2 + p[i+1])
    {
      if (p[i] == TAGGED_PARAMS_CH_ID)
      {
        // parse CH
        ch = p[i+2];
        break;
      }
    }

    if (ch == 0 || ch > 14)
    {
      // couldn't determine (valid) channel (can that happen?)
      // so stop here so we don't crash
#ifdef DEBUG
      os_printf("WARNING: couldn't get APs channel, not recording it\n");
#endif
      return;
    }

    for (uint16_t i=0; i<sniffed_aps_len[ch]; i++)
    {
      if (!os_memcmp(sniffed_aps[ch][i].bssid, hdr->addr2, 6))
      {
        // matching MACS
        // if we land here, we are done with parsing this packet
        return;
      }
    }

    // read SSID from beacon packet
    // to save with BSSID
    uint8_t ssid[33] = {0};
    for (uint16_t i = TAGGED_PARAMS_START; i < 112; i+= 2 + p[i+1])
    {
      if (p[i] == TAGGED_PARAMS_SSID_ID)
      {
        // parse SSID
        uint8_t ssid_len = p[i+1] < 33 ? p[i+1] : 32;
        os_memcpy(ssid, &p[i+2], ssid_len);
        ssid[ssid_len] = '\0';
        break;
      }
    }

    EXTEND_SPACE_IF_NEEDED(sniffed_aps[ch],sniffed_aps_len[ch],access_point);
    /*
    //if (sniffed_aps_len[ch] > 0 && sniffed_aps_len[ch] % MALLOC_PIECE == 0)
    if (sniffed_aps_len[ch] % MALLOC_PIECE == 0)
    {
      // realloc to make room for new ap struct
      sniffed_aps[ch] = os_realloc(sniffed_aps[ch], sniffed_aps_len[ch]*sizeof(access_point) + MALLOC_PIECE * sizeof(access_point));
    }
    */

    access_point *ap = sniffed_aps[ch] + sniffed_aps_len[ch];
    os_memcpy(ap->bssid, hdr->addr2, 6);
    os_strncpy(ap->ssid, ssid, os_strlen(ssid)+1); //include trailing null character
    ap->channel = ch;
    sniffed_aps_len[ch]++;

    /*
    os_memcpy(&sniffed_bssids[sniffed_bssids_number], hdr->addr2, 6);
    sniffed_bssids_number++;
    */
    /*
#ifdef DEBUG
    os_printf(" SA (AP): ");
    os_printf(MACSTR, MAC2STR(hdr->addr2));
    os_printf("\n");
#endif
    */
    return;
  }

  // parse data packet to fill sniffed_macs[]
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
        &&
       ! (
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
      // record pair if not already recorded
      for (uint16_t i=0; i<sniffed_macs_len[sniff_channel]; i++)
      {
        data_addr_pair *dap = sniffed_macs[sniff_channel] + i;
        if (
            (!(os_memcmp(dap->SA, hdr->addr1, 6)) && !(os_memcmp(dap->RA, hdr->addr2, 6)))
            ||
            (!(os_memcmp(dap->SA, hdr->addr2, 6)) && !(os_memcmp(dap->RA, hdr->addr1, 6)))
           )
        {
          // match; we have already recorded this pair
          return;
        }
      }
      for (uint16_t i=0; i<sniffed_stations_len[sniff_channel]; i++)
      {
        station *sta = sniffed_stations[sniff_channel] + i;
        if (
            !(os_memcmp(sta->mac, hdr->addr1, 6)) 
            || 
            !(os_memcmp(sta->mac, hdr->addr2, 6)) 
           )
        {
          // already identified this station
          return;
        }
      }

      // record the pair
      EXTEND_SPACE_IF_NEEDED(sniffed_macs[sniff_channel],
          sniffed_macs_len[sniff_channel], data_addr_pair);
      /*
      //if (sniffed_macs_len[sniff_channel] > 0 && sniffed_macs_len[sniff_channel] % MALLOC_PIECE == 0)
      if (sniffed_macs_len[sniff_channel] % MALLOC_PIECE == 0)
      {
        // realloc to make room for new data_addr_pair struct
        sniffed_macs[sniff_channel] = os_realloc(sniffed_macs[sniff_channel], 
                sniffed_macs_len[sniff_channel]*sizeof(data_addr_pair) + 
                MALLOC_PIECE * sizeof(data_addr_pair));
      }
      */

      data_addr_pair *dap = sniffed_macs[sniff_channel] + sniffed_macs_len[sniff_channel];
      os_memcpy(dap->SA, hdr->addr1, 6);
      os_memcpy(dap->RA, hdr->addr2, 6);
      sniffed_macs_len[sniff_channel]++;


      /*
      os_memcpy(&sniffed_macs[sniffed_macs_number], hdr->addr2, 6);
      sniffed_macs_number++;
#ifdef DEBUG
      os_printf(" SA: ");
      os_printf(MACSTR, MAC2STR(hdr->addr2));
      os_printf("\n");
#endif
      */
      //}
    }
  }
}

void ICACHE_FLASH_ATTR user_init()
{
  ssd1306_init();

  // enable serial debugging
  //uart_div_modify(0, UART_CLK_FREQ / 115200);
#ifdef DEBUG
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
#endif

/*
  // init sniffed_aps[] with list of 10 elements for each channel
  // realloc happens in packet handler function
  for (uint8_t i=1; i<15; i++)
  {
    sniffed_aps[i] = os_malloc(MALLOC_PIECE * sizeof(access_point));
    sniffed_macs[i] = os_malloc(MALLOC_PIECE * sizeof(data_addr_pair));
  }
*/

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
  os_timer_setfn(&some_timer, (os_timer_func_t *)scan_prepare, NULL);
  os_timer_arm(&some_timer, 3000, 0);
}



