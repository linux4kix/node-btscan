#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

int lastSignal = 10;
int debug = 0;

#define POLL_TIMEOUT 11000
#define MAX_DELAY    2000

static void signalHandler(int signal) {
  lastSignal = signal;
}

/* Try to active RSSI on inquiry, but if it doesn't work oh well. */
static void activate_rssi(int dd)
{
  write_inquiry_mode_cp cp;
  int err;

  cp.mode = 1;
  err = hci_send_cmd(dd, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE, WRITE_INQUIRY_MODE_RP_SIZE, &cp);
  if (debug) fprintf(stderr,"activate_rssi: err=%d\n",err);
  /* No other error checking, since this may fail and we don't care. */
}

int exit_periodic_inq(int dd)
{
        int err = hci_send_cmd(dd, OGF_LINK_CTL, OCF_EXIT_PERIODIC_INQUIRY,
                0, 0);
        return err;
}

int start_periodic_inq(int dd)
{
        int err;
        uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
        periodic_inquiry_cp cp;
        memset(&cp, 0, sizeof(cp));
        memcpy(&cp.lap, lap, 3);
        cp.max_period = htobs(0x31);
        cp.min_period = htobs(0x30);
        cp.length  = 0x2f;
        cp.num_rsp = 0x00;
        err = hci_send_cmd(dd, OGF_LINK_CTL, OCF_PERIODIC_INQUIRY,
                PERIODIC_INQUIRY_CP_SIZE, &cp);
        return err;
}

int main(int argc, const char* argv[])
{
  int hciDeviceId = 0;
  int hciSocket;
  struct hci_dev_info hciDevInfo;

  struct hci_filter oldHciFilter;
  struct hci_filter newHciFilter;
  socklen_t oldHciFilterLen;

  int previousAdapterState = -1;
  int currentAdapterState;
  const char* adapterState = NULL;
  
  fd_set rfds;

  unsigned char hciEventBuf[HCI_MAX_EVENT_SIZE], *ptr;
  int len;
  inquiry_info_with_rssi *inq_rssi;
  hci_event_hdr *hdr;
  char btAddress[18];

  memset(&hciDevInfo, 0x00, sizeof(hciDevInfo));

  // setup signal handlers
//  signal(SIGINT, signalHandler);
  signal(SIGKILL, signalHandler);
  signal(SIGUSR1, signalHandler);
  signal(SIGUSR2, signalHandler);
  signal(SIGHUP, signalHandler);

  prctl(PR_SET_PDEATHSIG, SIGINT);

  // remove buffering 
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  if (argc > 1)
    hciDeviceId = atoi(argv[1]);
  if (!hciDeviceId) {
    // Use the first available device
    hciDeviceId = hci_get_route(NULL);
  }

  if (hciDeviceId < 0) {
    hciDeviceId = 0; // use device 0, if device id is invalid
  }

  // setup HCI socket
  hciSocket = hci_open_dev(hciDeviceId);

  if (hciSocket == -1) {
    printf("adapterState unsupported\n");
    return -1;
  }
  hciDevInfo.dev_id = hciDeviceId;

  // get old HCI filter
  oldHciFilterLen = sizeof(oldHciFilter);
  getsockopt(hciSocket, SOL_HCI, HCI_FILTER, &oldHciFilter, &oldHciFilterLen);

  // setup new HCI filter
  hci_filter_clear(&newHciFilter);
  hci_filter_set_ptype(HCI_EVENT_PKT, &newHciFilter);
  hci_filter_set_event(EVT_INQUIRY_RESULT, &newHciFilter);
  hci_filter_set_event(EVT_INQUIRY_RESULT, &newHciFilter);
  hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &newHciFilter);
  hci_filter_set_event(EVT_INQUIRY_COMPLETE, &newHciFilter);
  setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &newHciFilter, sizeof(newHciFilter));

  activate_rssi(hciSocket);

  exit_periodic_inq(hciSocket);

  if (start_periodic_inq(hciSocket)) {
    perror("error while calling start_periodic_inq\n");
    exit(1);
  }

  while(1) {
    FD_ZERO(&rfds);
    FD_SET(hciSocket, &rfds);

    // get HCI dev info for adapter state
    ioctl(hciSocket, HCIGETDEVINFO, (void *)&hciDevInfo);
    currentAdapterState = hci_test_bit(HCI_UP, &hciDevInfo.flags);

    if (previousAdapterState != currentAdapterState) {
      previousAdapterState = currentAdapterState;

      if (!currentAdapterState) {
        adapterState = "poweredOff";
      } else {
        adapterState = "poweredOn";
      }

      printf("adapterState %s\n", adapterState);
    }

    // read event
    while ((len = read(hciSocket, hciEventBuf, sizeof(hciEventBuf))) < 0) {
      if (errno == EAGAIN || errno == EINTR) {
        printf("continue\n");
        continue;
      }
      printf("goto failed\n");
      goto failed;
    }
    hdr = (void *) (hciEventBuf + 1);
    ptr = hciEventBuf + (2 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);
    switch (hdr->evt) {
      case EVT_INQUIRY_COMPLETE:
        if (start_periodic_inq(hciSocket)) {
          perror("error while calling start_periodic_inq\n");
          exit(1);
        }
      break;
      case EVT_INQUIRY_RESULT_WITH_RSSI:
        if (len != 15) {
          perror("got inq rssi but wrong length (should be 15)\n");
          continue;
        }
        inq_rssi = (inquiry_info_with_rssi *)ptr;
        ba2str(&inq_rssi->bdaddr, btAddress);
        printf ("result %s, %d\n",btAddress,inq_rssi->rssi);
      }
    }

failed:
  // restore original filter
  setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &oldHciFilter, sizeof(oldHciFilter));

  close(hciSocket);

  return 0;
}
