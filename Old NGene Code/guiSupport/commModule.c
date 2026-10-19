/*
 * @file   CommModule.c
 * @author Intecs
 * @date December 4, 2007, 6:04 PM
 * @version 0.1
 *
 * @brief Communication Module
 * @details This is the Communication Module implementation file.
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netdb.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <time.h>
#include <sys/time.h>

#include <semaphore.h>

#include "commModule.h"
#include "threadutils.h"
#include "generic.h"
#include "ngeneConfiguration.h"
#include "ngeneConstants.h"


//#define _COMM_MODULE_DEBUG

///////////////////////// Private variables /////////////////////////
/** Transmitting thread. */
pthread_t	tx_pthread_id;
/** Receiving thread. */
pthread_t	rx_pthread_id;

/** Initialization function */
int (*user_elab_initialize)(void) = 0;
/** Finalization function */
int (*user_elab_terminate)(void) = 0;

/** The current status of the Communication module */
enum ComStatus_t status;

/** Mutex for the status variable */
pthread_mutex_t	status_mutex = PTHREAD_MUTEX_INITIALIZER;
/** Condition on status for threads synchronization */
pthread_cond_t	status_cond  = PTHREAD_COND_INITIALIZER;
/** Semaphore on send requests */
sem_t send_semaphore;

/** It is true only at startup. It is used to control the execution of
 * the ComGetStatus() function.
 */
boolean first_execution = COMM_T;

/** buffer that will hold the data to be sent to the client. */
void *data_buffer = 0;
/** buffer that will hold the spectrum data to be sent to the client. */
void *spectrum_buffer = 0;
/** buffer that will hold the histogram data to be sent to the client. */
void *histogram_buffer = 0;
/** buffer that will hold the ephemeris data to be sent to the client. */
void *ephemeris_buffer = 0;
/** buffer that will hold the configuration parameters when they will be
 * received from the server. */
void **user_config_buffer = 0;

/** length in bytes of the data buffer */
long long data_length = 0;
/** length in bytes of the spectrum data buffer */
long long spectrum_length = 0;
/** length in bytes of the histogram data buffer */
long long histogram_length = 0;
/** length in bytes of the ephemeris data buffer */
long long ephemeris_length = 0;

/** it is true iff the data buffer is ready to be sent to the client */
boolean data_ready = COMM_F;
/** it is true iff the spectrum data buffer is ready to be sent to the client */
boolean spectrum_ready = COMM_F;
/** it is true iff the histogram data buffer is ready to be sent to the client */
boolean histogram_ready = COMM_F;
/** it is true iff the ephemeris data buffer is ready to be sent to the client */
boolean ephemeris_ready = COMM_F;

/** mutex structure for a mutual exclusive access to the buffer of data */
pthread_mutex_t	data_mutex      = PTHREAD_MUTEX_INITIALIZER;
/** mutex structure for a mutual exclusive access to the buffer of spectrum data */
pthread_mutex_t	spectrum_mutex  = PTHREAD_MUTEX_INITIALIZER;
/** mutex structure for a mutual exclusive access to the buffer of histogram data */
pthread_mutex_t	histogram_mutex = PTHREAD_MUTEX_INITIALIZER;
/** mutex structure for a mutual exclusive access to the buffer of ephemeris data */
pthread_mutex_t	ephemeris_mutex = PTHREAD_MUTEX_INITIALIZER;

/** It is true iff the spectrum has been requested by the client */
boolean spectrum_on = COMM_F;
/** It is true iff the histogram has been requested by the client */
boolean histogram_on = COMM_F;

///////////////////////// Socket communication definitions /////////////////////////

pthread_mutex_t	sock_mutex = PTHREAD_MUTEX_INITIALIZER;
int sockfd, newsockfd, portno;
unsigned int clilen;
struct sockaddr_in serv_addr, cli_addr;
struct hostent *host_remote;


#define SOH         0x01
#define MAXDATALEN  (RX_BUFFER_LEN - 15)

/** Message structure.
 *  | SOH | CMD | ~CMD |
 *  LENHHH | LENHHL | LENHLH | LENHLL | LENLHH | LENLHL | LENLLH | LENLLL | ... | ... |
 *    ... | CHKSUMHH | CHKSUMHL | CHKSUMLH | CHKSUMLL |
 *
 */
typedef struct {
  unsigned char        soh;
  unsigned char        cmd;
  unsigned char        cmdcpl;
  unsigned long long   len;
  unsigned char        data[MAXDATALEN];
  unsigned int         chksum;
} msg_t;


///////////////////////// Private Functions /////////////////////////
/** transmitting thread main function.
 * @param ptr pointer to a void buffer.
 */
void *tx_pthread_func (void *ptr);

/** receiving thread main function.
 * @param ptr pointer to a void buffer.
 */
void *rx_pthread_func (void *ptr);

/** getter method for the Communication module status.
 * @return the current status.
 */
enum ComStatus_t get_status (void);

/** allocate memory for the received configuration and put it there by taking
 * it from the received message.
 * @return 0 in case the configuration is updated, -1 in error cases.
 */
int update_configuration(msg_t *msg);

/** Send an ack the client referring to the specified message.
 * @param msg the message to which the ack refers.
 * @return 0 if no error happens.
 */
int commit_ack (msg_t *msg);

/** Send a nack the client referring to the specified message.
 * @param msg the message to which the nack refers.
 * @return 0 if no error happens.
 */
int commit_nack (msg_t *msg);

/** Send a data packet to the client.
 * @return 0 if no error happens.
 */
int commit_data ();

/** Send a spectrum packet to the client.
 * @return 0 if no error happens.
 */
int commit_spectrum ();

/** Send a histogram packet to the client.
 * @return 0 if no error happens.
 */
int commit_histogram ();

/** Send an ephemeris packet to the client.
 * @return 0 if no error happens.
 */
int commit_ephemeris();

/** Send file to the client.
 * @param filename the name of the file to be transferred.
 * @param filetype the type of file to be sent. It can be
 *        HIGHCORRCHUNK or RAWSAMPLESCHUNK.
 * @return 0 if no error happens, a negative value otherwise.
 */
int commit_file (char *filename, int filetype);

///////////////////////// Public Functions /////////////////////////

///////////////////////////
// ComSetPort
///////////////////////////
void ComSetPort(int port) {
#ifdef _COMM_MODULE_DEBUG
    fprintf(stdout, "*** Set Port: %d\n", port);
#endif
    portno = port;
}

///////////////////////////
// ComSetInitFunction
///////////////////////////
void ComSetInitFunction(int (*elab_initialize)(void)) {
    // Assign functions to initialize elaborations
    user_elab_initialize = elab_initialize;
}

///////////////////////////
// ComSetTerminateFunction
///////////////////////////
void ComSetTerminateFunction(int (*elab_terminate)(void)) {
    // Assign functions to terminate elaborations
    user_elab_terminate = elab_terminate;
}

///////////////////////////
// ComSetConfigBuffer
///////////////////////////
void ComSetConfigBuffer(void** buffer) {
    user_config_buffer = buffer;
}

///////////////////////////
// ComStart
///////////////////////////
void ComStart (void)
{
  pthread_mutex_lock (&status_mutex);
  status = STANDBY;
  pthread_mutex_unlock (&status_mutex);

  first_execution = COMM_T;

  data_ready      = COMM_F;
  spectrum_ready  = COMM_F;
  histogram_ready = COMM_F;
  ephemeris_ready = COMM_F;

  spectrum_on     = COMM_F;
  histogram_on    = COMM_F;

  sem_init(&send_semaphore, 0 ,0);

  generate_pthread (&rx_pthread_id, rx_pthread_func, 20);
  generate_pthread (&tx_pthread_id, tx_pthread_func, 20);

  return;
}

///////////////////////////
// ComStop
///////////////////////////
void ComStop (void) {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** ComStop\n");
#endif

  sem_destroy(&send_semaphore);

  return;
}

///////////////////////////
// ComGetStatus
///////////////////////////
enum ComStatus_t ComGetStatus (void) {

  enum ComStatus_t mystatus;

  pthread_mutex_lock (&status_mutex);
  if (status != RUNNING) {
    if (status == STANDBY) {
      if (!first_execution) {
        if (user_elab_terminate) {

#ifdef _COMM_MODULE_DEBUG
       fprintf(stdout, "Call to user_elab_terminate.\n");
#endif

          user_elab_terminate ();
        }
      } else {
        first_execution = COMM_F;
      }

      pthread_cond_wait (&status_cond, &status_mutex);

      if (status == RUNNING)
          if (user_elab_initialize) {
	    // This is the first execution after the start command
	    // Re-initialize variables.
	    data_ready      = COMM_F;
	    spectrum_ready  = COMM_F;
	    histogram_ready = COMM_F;
	    ephemeris_ready = COMM_F;
	    spectrum_ready  = COMM_F;
	    histogram_ready = COMM_F;
	    spectrum_on     = COMM_F;
	    histogram_on    = COMM_F;

            user_elab_initialize ();
          }
    }
  }
  mystatus = status;
  pthread_mutex_unlock (&status_mutex);

  return mystatus;
}

///////////////////////////
// ComSendData
///////////////////////////
int ComSendData (void *buffer, int length) {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** ComSendData\n");
#endif

  data_buffer = buffer;
  data_length = length;
  data_ready = COMM_T;
  sem_post(&send_semaphore);

  return 0;
}

///////////////////////////
// ComSendSpectrum
///////////////////////////
int ComSendSpectrum (void *buffer, int length) {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "ComSendSpectrum\n");
#endif

  spectrum_buffer = buffer;
  spectrum_length = length;
  spectrum_ready = COMM_T;
  sem_post(&send_semaphore);

  return 0;
}

///////////////////////////
// ComSendHistogram
///////////////////////////
int ComSendHistogram (void *buffer, int length) {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** ComSendHistogram\n");
#endif

  histogram_buffer = buffer;
  histogram_length = length;
  histogram_ready = COMM_T;
  sem_post(&send_semaphore);

  return 0;
}

///////////////////////////
// ComSendEphemeris
///////////////////////////
int ComSendEphemeris(void *buffer, int length)  {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** ComSendEphemeris\n");
#endif

  ephemeris_buffer = buffer;
  ephemeris_length = length;
  ephemeris_ready = COMM_T;
  sem_post(&send_semaphore);

  return 0;
}

///////////////////////////
// ComSendHighCorrFile
///////////////////////////
int ComSendHighCorrFile       (char *pathname) {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** ComSendHighCorrFile\n");
#endif

  int result = commit_file (pathname, HIGHCORRCHUNK);
  //sem_post(&send_semaphore);

  return result;
}

///////////////////////////
// ComSendRawSamplesFile
///////////////////////////
int ComSendRawSamplesFile       (char *pathname) {
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** ComSendRawSamplesFile\n");
#endif

  int result = commit_file (pathname, RAWSAMPLESCHUNK);
  //sem_post(&send_semaphore);

  return result;
}

///////////////////////////
// ComLockDataArea
///////////////////////////
void ComLockDataArea (void) {
  pthread_mutex_lock (&data_mutex);
  return;
}

///////////////////////////
// ComLockSpectrumArea
///////////////////////////
void ComLockSpectrumArea (void) {
  pthread_mutex_lock (&spectrum_mutex);
  return;
}

///////////////////////////
// ComLockHistogramArea
///////////////////////////
void ComLockHistogramArea (void) {
  pthread_mutex_lock (&histogram_mutex);
  return;
}

///////////////////////////
// ComLockEphemerisArea
///////////////////////////
void ComLockEphemerisArea (void) {
  pthread_mutex_lock (&ephemeris_mutex);
  return;
}

///////////////////////////
// ComUnlockDataArea
///////////////////////////
void ComUnlockDataArea (void) {
  pthread_mutex_unlock (&data_mutex);
  return;
}

///////////////////////////
// ComUnlockSpectrumArea
///////////////////////////
void ComUnlockSpectrumArea (void) {
  pthread_mutex_unlock (&spectrum_mutex);
  return;
}

///////////////////////////
// ComUnlockHistogramArea
///////////////////////////
void ComUnlockHistogramArea (void) {
  pthread_mutex_unlock (&histogram_mutex);
  return;
}

///////////////////////////
// ComUnlockEphemerisArea
///////////////////////////
void ComUnlockEphemerisArea (void) {
  pthread_mutex_unlock (&ephemeris_mutex);
  return;
}

///////////////////////////
// ComSpectrumOn
///////////////////////////
boolean ComSpectrumOn (void) {

  return spectrum_on;
}

///////////////////////////
// ComHistogramOn
///////////////////////////
boolean ComHistogramOn (void) {

  return histogram_on;
}


///////////////////////// Private Functions /////////////////////////

///////////////////////////
// get_status
///////////////////////////
enum ComStatus_t get_status (void) {

  enum ComStatus_t mystatus;

  pthread_mutex_lock (&status_mutex);
  if (status != RUNNING)
    if (status == STANDBY)
      pthread_cond_wait (&status_cond, &status_mutex);
  mystatus = status;
  pthread_mutex_unlock (&status_mutex);

  return mystatus;
};

int read_msg (int sockfd, msg_t *msg) {

  unsigned char buffer_rx[RX_BUFFER_LEN];
  unsigned int checksum = 0;
  int j, n;
  struct timeval timeout;
  int minByteOpt;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** read_msg\n");
#endif

  bzero (buffer_rx, RX_BUFFER_LEN);
  msg->cmd = (unsigned char) DUMMYCODE;

  /** read only soh */
  n = recv (sockfd, buffer_rx, 1, MSG_WAITALL);
  if (n != 1)
    return (-1);

  /** Check soh */
  msg->soh = buffer_rx[0];
  if (buffer_rx[0] != SOH)
    return (-2);
  checksum += (unsigned int) msg->soh;

  /** read cmd, cmdcpl */
  n = recv (sockfd, &(buffer_rx[1]), 2, MSG_WAITALL);
  if (n != 2)
    return (-3);

  /** Check cmd */
  msg->cmd = buffer_rx[1];
  msg->cmdcpl = buffer_rx[2];
  if (msg->cmd != (unsigned char) (~(msg->cmdcpl)))
    return (-4);
  checksum += (unsigned int) msg->cmd;
  checksum += (unsigned int) msg->cmdcpl;

#ifdef _COMM_MODULE_DEBUG
  printf("cmd=%d\n", msg->cmd);
#endif

  /** read datalen */
  n = read (sockfd, &(buffer_rx[3]), 8);
  if (n != 8)
    return (-5);

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout,"\ndatalen buffer[3,4,5,6,7,8,9,10] = [0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]\n\n",
          buffer_rx[3],buffer_rx[4],buffer_rx[5],buffer_rx[6],
          buffer_rx[7],buffer_rx[8],buffer_rx[9],buffer_rx[10]);
#endif

  /** Check datalen */
  msg->len = (((unsigned long long) buffer_rx[3]) << 56) +
          (((unsigned long long) buffer_rx[4]) << 48) +
          (((unsigned long long) buffer_rx[5]) << 40) +
          (((unsigned long long) buffer_rx[6]) << 32) +
          (((unsigned long long) buffer_rx[7]) << 24) +
          (((unsigned long long) buffer_rx[8]) << 16) +
          (((unsigned long long) buffer_rx[9]) << 8) +
          ((unsigned long long) buffer_rx[10]);

  if (msg->len > MAXDATALEN)
    return (-6);
  checksum += (unsigned int) buffer_rx[3];
  checksum += (unsigned int) buffer_rx[4];
  checksum += (unsigned int) buffer_rx[5];
  checksum += (unsigned int) buffer_rx[6];
  checksum += (unsigned int) buffer_rx[7];
  checksum += (unsigned int) buffer_rx[8];
  checksum += (unsigned int) buffer_rx[9];
  checksum += (unsigned int) buffer_rx[10];

  /** read data + chksum. Consider that the length of messages from Client is
   * hold by an integer.
   */
  minByteOpt = (unsigned int)(msg->len + 4);
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &minByteOpt, 4);
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
  n = read (sockfd, &(buffer_rx[11]), (unsigned int)(msg->len + 4));
  minByteOpt = 1;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVLOWAT, &minByteOpt, 4);
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

  if ((unsigned long long)n != msg->len + 4)
    return (-7);

  for (j=0; (unsigned long long)j<(msg->len); j++) {
    msg->data[j] = buffer_rx[11+j];
    checksum += (unsigned int) msg->data[j];
  }

  msg->chksum = (((unsigned int) buffer_rx[11+j]) << 24) +
          (((unsigned int) buffer_rx[11+j+1]) << 16) +
          (((unsigned int) buffer_rx[11+j+2]) << 8) +
          ((unsigned int) buffer_rx[11+j+3]);

  if (msg->chksum != checksum) {
#ifdef _COMM_MODULE_DEBUG
    fprintf (stdout, "msg->chksum = %u, checksum = %u\n", msg->chksum, checksum);
#endif
    return (-8);
  }


  return (0);
}

///////////////////////////
// tx_pthread_func
///////////////////////////
void *tx_pthread_func (void *ptr) {


  while (get_status () == RUNNING) {

    sem_wait(&send_semaphore);

    if (data_ready) {
#ifdef _COMM_MODULE_DEBUG
      fprintf(stdout, "COMMIT DATA------------------------------\n");
#endif
      commit_data ();
      data_ready = COMM_F;
    }

    if (spectrum_ready) {
#ifdef _COMM_MODULE_DEBUG
      fprintf(stdout, "COMMIT SPECTRUM------------------------------\n");
#endif
      commit_spectrum ();
      spectrum_ready = COMM_F;
    }

    if (histogram_ready) {
#ifdef _COMM_MODULE_DEBUG
      printf("COMMIT HISTOGRAM------------------------------\n");
#endif
      commit_histogram ();
      histogram_ready = COMM_F;
    }

    if (ephemeris_ready) {
        commit_ephemeris ();
        ephemeris_ready = COMM_F;
    }
  }

  ptr = NULL;
  return (ptr);
}

///////////////////////////
// rx_pthread_func
///////////////////////////
void *rx_pthread_func (void *ptr) {

  msg_t msg;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** rx_pthread_func ***\n");
#endif

  /**
   *  initialize rx socket
   */
  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
#ifdef _COMM_MODULE_DEBUG
    fprintf(stdout, "ERROR opening rx socket\n");
#endif
    return (NULL);
  }

  bzero ((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons (portno);

  if (bind (sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
#ifdef _COMM_MODULE_DEBUG
    fprintf(stdout, "ERROR on rx binding\n");
#endif
    return (NULL);
  }

  listen (sockfd, 1);
  clilen = sizeof(cli_addr);

  newsockfd = 0;

  while (1) {

#ifdef _COMM_MODULE_DEBUG
    int i;
#endif
    int res;

#ifdef _COMM_MODULE_DEBUG
    fprintf(stdout, "--------------- RECEIVE LOOP ----------------------------\n");
#endif

    sleep (1);

#ifdef _COMM_MODULE_DEBUG
    fprintf(stdout, "Accepting a new connection\n");
#endif

    newsockfd = accept (sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {

#ifdef _COMM_MODULE_DEBUG
      fprintf(stdout, "***** ERROR ON RX ACCEPT *****\n");
#endif

      return (NULL);
    }

    // Read requests and serve them until there's an error (e.g. the client closes
    // the connection.
    int receiveError = 0;
    while (!receiveError) {

        //  read from socket
        if ((res = read_msg (newsockfd, &msg)) < 0) {
    #ifdef _COMM_MODULE_DEBUG
          fprintf (stdout, "\nread_msg returned: %d\n", res);
          fflush (stdout);
    #endif
          receiveError = 1;
          continue;
        }

    #ifdef _COMM_MODULE_DEBUG
        fprintf (stdout, "received msg %02u: 0x%02x 0x%02x   len=%d\n",
                msg.soh, msg.cmd, msg.cmdcpl, (unsigned int)msg.len);
        fprintf (stdout, "               : ");
        for (i=0; i<msg.len; i++) fprintf (stdout, "0x%02x ", msg.data[i]);
        fprintf (stdout, "(chksum:%u)\n\n", msg.chksum);
    #endif

        switch ((enum ComMsgCodes_t) msg.cmd) {

          case APPLEXIT:
          //////////////////// APPLEXIT ////////////////////
    #ifdef _COMM_MODULE_DEBUG
            fprintf(stdout, "APPLEXIT request received\n");
    #endif
            pthread_mutex_lock (&status_mutex);
            status = EXITING;
            pthread_cond_broadcast (&status_cond);
            pthread_mutex_unlock (&status_mutex);
            return (NULL);
            break;

          case ELABSTART:
          //////////////////// ELABSTART ////////////////////
    #ifdef _COMM_MODULE_DEBUG
            fprintf(stdout, "ELABSTART request received\n");
    #endif
            pthread_mutex_lock (&status_mutex);
            status = RUNNING;
            pthread_cond_broadcast (&status_cond);
            pthread_mutex_unlock (&status_mutex);
            commit_ack (&msg);
            break;

          case ELABSTOP:
          //////////////////// ELABSTOP ///////////////////
    #ifdef _COMM_MODULE_DEBUG
            fprintf(stdout, "ELABSTOP request received\n");
    #endif
            pthread_mutex_lock (&status_mutex);
            status = STANDBY;
            pthread_cond_broadcast (&status_cond);
            pthread_mutex_unlock (&status_mutex);
            commit_ack (&msg);
            break;

          case CONFIG:
          //////////////////// CONFIG ////////////////////
    #ifdef _COMM_MODULE_DEBUG
            fprintf(stdout, "CONFIG request received\n");
    #endif
            pthread_mutex_lock (&status_mutex);
            if (status == STANDBY) {

              //  update configuration
              update_configuration(&msg);

              commit_ack (&msg);

            } else {

              commit_nack (&msg);
            }
            pthread_mutex_unlock (&status_mutex);
            break;

          case SPECTON:
          //////////////////// SPECTRUM ON ////////////////////
            spectrum_on = COMM_T;
            commit_ack (&msg);
            break;

          case SPECTOFF:
          //////////////////// SPECTRUM OFF ////////////////////
            spectrum_on = COMM_F;
            commit_ack (&msg);
            break;

          case HISTON:
          //////////////////// HISTOGRAM ON ////////////////////
            histogram_on = COMM_T;
            commit_ack (&msg);
            break;

          case HISTOFF:
          //////////////////// HISTOGRAM OFF ////////////////////
            histogram_on = COMM_F;
            commit_ack (&msg);
            break;

          default:
            commit_nack (&msg);
            break;
        }
    } // while (!receiveError)

    // Reset the status to STANDBY
#ifdef _COMM_MODULE_DEBUG
       fprintf(stdout, "Resetting status to STANDBY\n");
#endif
    pthread_mutex_lock (&status_mutex);
    status = STANDBY;
    pthread_mutex_unlock (&status_mutex);
#ifdef _COMM_MODULE_DEBUG
       fprintf(stdout, "Current status: STANDBY\n");
#endif

    // close the socket to the client
    if (newsockfd) {
#ifdef _COMM_MODULE_DEBUG
       fprintf(stdout, "Closing connection\n");
#endif
       close(newsockfd);
       newsockfd = 0;
    }

  } // while (1)
  ptr = NULL;
  return (ptr);
}

///////////////////////////
// update_configuration
///////////////////////////
int update_configuration(msg_t *msg) {
    int i;

    if (user_config_buffer == 0) {
        return -1;
    }

    if((*user_config_buffer) == NULL) /* MN 2014: do not allocate if already allocated! */
    {
    	*user_config_buffer = malloc(msg->len);
    }

#ifdef _COMM_MODULE_DEBUG
    printf("*user_config_buffer=%p\n", *user_config_buffer);
    /**/
#endif

    if (*user_config_buffer) {
        for (i=0; (unsigned long long)i<msg->len; i++) {
            (*((unsigned char **)user_config_buffer))[i] = (unsigned char)(msg->data[i]);
        }
    } else {
        return -1;
    }

    return 0;
}

///////////////////////////
// commit_ack
///////////////////////////
int commit_ack (msg_t *msg) {

  /**
   *  form ack msg and send it to socket
   */

  unsigned char buffer[16];
  unsigned int checksum = 0;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** commit_ack\n");
#endif

  /** SOH */
  buffer[0] = (unsigned char) SOH;
  checksum += (unsigned int) buffer[0];
  /** cmd, cmdcpl */
  buffer[1] = (unsigned char) ACK;
  checksum += (unsigned int) buffer[1];
  buffer[2] = ~(buffer[1]);
  checksum += (unsigned int) buffer[2];
  /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
  buffer[3] = 0;
  checksum += (unsigned int) buffer[3];
  buffer[4] = 0;
  checksum += (unsigned int) buffer[4];
  buffer[5] = 0;
  checksum += (unsigned int) buffer[5];
  buffer[6] = 0;
  checksum += (unsigned int) buffer[6];
  buffer[7] = 0;
  checksum += (unsigned int) buffer[7];
  buffer[8] = 0;
  checksum += (unsigned int) buffer[8];
  buffer[9] = 0;
  checksum += (unsigned int) buffer[9];
  buffer[10] = 1;
  checksum += (unsigned int) buffer[10];
  /** payload */
  buffer[11] = msg->cmd;
  checksum += (unsigned int) buffer[11];

  /** chksumHH, chksumHL, chksumLH, chksumLL */
  buffer[12]  = (unsigned char) ((checksum & 0xFF000000) >> 24);
  buffer[13]  = (unsigned char) ((checksum & 0x00FF0000) >> 16);
  buffer[14] = (unsigned char) ((checksum & 0x0000FF00) >> 8);
  buffer[15] = (unsigned char) ((checksum & 0x000000FF));

  pthread_mutex_lock (&sock_mutex);
  write (newsockfd, buffer, 16);
  pthread_mutex_unlock (&sock_mutex);

  return 0;
}

///////////////////////////
// commit_nack
///////////////////////////
int commit_nack (msg_t *msg) {

  /**
   *  form nack msg and send it to socket
   */

  unsigned char buffer[16];
  unsigned int checksum = 0;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** commit_nack\n");
#endif

  /** SOH */
  buffer[0] = (unsigned char) SOH;
  checksum += (unsigned int) buffer[0];

  /** cmd, cmdcpl */
  buffer[1] = (unsigned char) NACK;
  checksum += (unsigned int) buffer[1];
  buffer[2] = ~(buffer[1]);
  checksum += (unsigned int) buffer[2];

  /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
  buffer[3] = 0;
  checksum += (unsigned int) buffer[3];
  buffer[4] = 0;
  checksum += (unsigned int) buffer[4];
  buffer[5] = 0;
  checksum += (unsigned int) buffer[5];
  buffer[6] = 0;
  checksum += (unsigned int) buffer[6];
  buffer[7] = 0;
  checksum += (unsigned int) buffer[7];
  buffer[8] = 0;
  checksum += (unsigned int) buffer[8];
  buffer[9] = 0;
  checksum += (unsigned int) buffer[9];
  buffer[10] = 1;
  checksum += (unsigned int) buffer[10];

  /** payload */
  buffer[11] = msg->cmd;
  checksum += (unsigned int) buffer[11];

  /** chksumHH, chksumHL, chksumLH, chksumLL */
  buffer[12]  = (unsigned char) ((checksum & 0xFF000000) >> 24);
  buffer[13]  = (unsigned char) ((checksum & 0x00FF0000) >> 16);
  buffer[14] = (unsigned char) ((checksum & 0x0000FF00) >> 8);
  buffer[15] = (unsigned char) ((checksum & 0x000000FF));

  pthread_mutex_lock (&sock_mutex);
  write (newsockfd, buffer, 16);
  pthread_mutex_unlock (&sock_mutex);

  return 0;
}

///////////////////////////
// commit_data
///////////////////////////
int commit_data () {

  unsigned char *buffer;
  long long bufferlen = 0;
  unsigned int checksum = 0;
  unsigned long tmpLen = 0;

  int i, j;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** commit_data\n");
#endif

  pthread_mutex_lock (&data_mutex);

  /**
   *  form data msg and send it to socket
   */

  bufferlen = data_length + 15;  // 15 is the nb of bytes needed
  // to store SOH, CMD, CMDcpl, len, chksum
  buffer = malloc (bufferlen);

  if (buffer) {

    /** soh */
    buffer[0] = (unsigned char) SOH;
    checksum += (unsigned int) buffer[0];

    /** cmd, cmdcpl */
    buffer[1] = (unsigned char) DATACHUNK;
    checksum += (unsigned int) buffer[1];
    buffer[2] = (unsigned char) (~(buffer[1]));
    checksum += (unsigned int) buffer[2];

    /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
    // write 4 bytes at a time (because of bits-operation problems)
    tmpLen = data_length >> 32;
    buffer[3] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[3];
    buffer[4] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[4];
    buffer[5] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[5];
    buffer[6] = (unsigned char)((tmpLen & 0x000000FF));
    checksum += (unsigned int) buffer[6];

    tmpLen = data_length & 0xFFFFFFFF;
    buffer[7] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[7];
    buffer[8] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[8];
    buffer[9] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[9];
    buffer[10] = (unsigned char)(tmpLen & 0x000000FF);
    checksum += (unsigned int) buffer[10];

    /** payload */
    for (j=0, i=11; j<data_length; j++, i++) {
      buffer[i] = (unsigned char) ((unsigned char *) data_buffer)[j];
      checksum += (unsigned int) buffer[i];
    }

    /** chksumHH, chksumHL, chksumLH, chksumLL */
    buffer[i]   = (unsigned char) ((checksum & 0xFF000000) >> 24);
    buffer[i+1] = (unsigned char) ((checksum & 0x00FF0000) >> 16);
    buffer[i+2] = (unsigned char) ((checksum & 0x0000FF00) >> 8);
    buffer[i+3] = (unsigned char) ((checksum & 0x000000FF));
  }
  pthread_mutex_unlock (&data_mutex);

  if (buffer) {
    pthread_mutex_lock (&sock_mutex);
    write (newsockfd, buffer, bufferlen);
    pthread_mutex_unlock (&sock_mutex);
  }


  free(buffer); /* MN 2014 */
  return 0;
}

///////////////////////////
// commit_spectrum
///////////////////////////
int commit_spectrum () {

  unsigned char *buffer;
  long long  bufferlen = 0;
  unsigned short checksum = 0;
  unsigned long tmpLen;

  int i, j;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "commit_spectrum\n");
#endif

  pthread_mutex_lock (&spectrum_mutex);

  /**
   *  form spectrum msg and send it to socket
   */

  bufferlen = spectrum_length + 15;  // 15 is the nb of bytes needed
  // to store SOH, CMD, CMDcpl, len, chksum
  buffer = malloc (bufferlen);

  if (buffer) {

    /** soh */
    buffer[0] = (unsigned char) SOH;
    checksum += (unsigned int) buffer[0];

    /** cmd, cmdcpl */
    buffer[1] = (unsigned char) SPECTRUMCHUNK;
    checksum += (unsigned int) buffer[1];
    buffer[2] = (unsigned char) (~(buffer[1]));
    checksum += (unsigned int) buffer[2];

    /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
    tmpLen = spectrum_length >> 32;
    buffer[3] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[3];
    buffer[4] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[4];
    buffer[5] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[5];
    buffer[6] = (unsigned char)((tmpLen & 0x000000FF));
    checksum += (unsigned int) buffer[6];

    tmpLen = spectrum_length & 0xFFFFFFFF;
    buffer[7] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[7];
    buffer[8] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[8];
    buffer[9] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[9];
    buffer[10] = (unsigned char)(tmpLen & 0x000000FF);
    checksum += (unsigned int) buffer[10];

    /** payload */
    for (j=0, i=11; j<spectrum_length; j++, i++) {
      buffer[i] = (unsigned char) ((unsigned char *) spectrum_buffer)[j];
      checksum += (unsigned int) buffer[i];
    }

    /** chksumHH, chksumHL, chksumLH, chksumLL */
    buffer[i]   = (unsigned char) ((checksum & 0xFF000000) >> 24);
    buffer[i+1] = (unsigned char) ((checksum & 0x00FF0000) >> 16);
    buffer[i+2] = (unsigned char) ((checksum & 0x0000FF00) >> 8);
    buffer[i+3] = (unsigned char) ((checksum & 0x000000FF));
  }

  pthread_mutex_unlock (&spectrum_mutex);

  if (buffer) {
    pthread_mutex_lock (&sock_mutex);
    write (newsockfd, buffer, bufferlen);
    pthread_mutex_unlock (&sock_mutex);

    // AM
    free(buffer);

  }

  return 0;
}

///////////////////////////
// commit_histogram
///////////////////////////
int commit_histogram () {

  unsigned char *buffer;
  long long bufferlen = 0;
  unsigned short checksum = 0;
  unsigned long tmpLen = 0;

  int i, j;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** commit_histogram\n");
#endif

  pthread_mutex_lock (&histogram_mutex);

  /**
   *  form histogram msg and send it to socket
   */

  bufferlen = histogram_length + 15;  // 15 is the nb of bytes needed
  // to store SOH, CMD, CMDcpl, len, chksum
  buffer = malloc (bufferlen);

  if (buffer) {

    /** soh */
    buffer[0] = (unsigned char) SOH;
    checksum += (unsigned int) buffer[0];

    /** cmd, cmdcpl */
    buffer[1] = (unsigned char) HISTOGRAMCHUNK;
    checksum += (unsigned int) buffer[1];
    buffer[2] = (unsigned char) (~(buffer[1]));
    checksum += (unsigned int) buffer[2];

    /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
    tmpLen = histogram_length >> 32;
    buffer[3] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[3];
    buffer[4] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[4];
    buffer[5] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[5];
    buffer[6] = (unsigned char)((tmpLen & 0x000000FF));
    checksum += (unsigned int) buffer[6];

    tmpLen = histogram_length & 0xFFFFFFFF;
    buffer[7] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[7];
    buffer[8] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[8];
    buffer[9] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[9];
    buffer[10] = (unsigned char)(tmpLen & 0x000000FF);
    checksum += (unsigned int) buffer[10];

    /** payload */
    for (j=0, i=11; j<histogram_length; j++, i++) {
      buffer[i] = (unsigned char) ((unsigned char *) histogram_buffer)[j];
      checksum += (unsigned int) buffer[i];
    }

    /** chksumHH, chksumHL, chksumLH, chksumLL */
    buffer[i]   = (unsigned char) ((checksum & 0xFF000000) >> 24);
    buffer[i+1] = (unsigned char) ((checksum & 0x00FF0000) >> 16);
    buffer[i+2] = (unsigned char) ((checksum & 0x0000FF00) >> 8);
    buffer[i+3] = (unsigned char) ((checksum & 0x000000FF));
  }

  pthread_mutex_unlock (&histogram_mutex);

  if (buffer) {
    pthread_mutex_lock (&sock_mutex);
    write (newsockfd, buffer, bufferlen);
    pthread_mutex_unlock (&sock_mutex);

    // AM
    free(buffer);
  }



  return 0;
}

///////////////////////////
// commit_ephemeris
///////////////////////////
int commit_ephemeris () {

  unsigned char *buffer;
  long long bufferlen = 0;
  unsigned short checksum = 0;
  unsigned long tmpLen = 0;

  int i, j;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "*** commit_ephemeris\n");
#endif

  pthread_mutex_lock (&ephemeris_mutex);

  /**
   *  form ephemeris msg and send it to socket
   */

  bufferlen = ephemeris_length + 15;  // 15 is the nb of bytes needed
  // to store SOH, CMD, CMDcpl, len, chksum
  buffer = malloc (bufferlen);

  if (buffer) {

    /** soh */
    buffer[0] = (unsigned char) SOH;
    checksum += (unsigned int) buffer[0];

    /** cmd, cmdcpl */
    buffer[1] = (unsigned char) EPHEMERISCHUNK;
    checksum += (unsigned int) buffer[1];
    buffer[2] = (unsigned char) (~(buffer[1]));
    checksum += (unsigned int) buffer[2];

    /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
    tmpLen = ephemeris_length >> 32;
    buffer[3] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[3];
    buffer[4] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[4];
    buffer[5] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[5];
    buffer[6] = (unsigned char)((tmpLen & 0x000000FF));
    checksum += (unsigned int) buffer[6];

    tmpLen = ephemeris_length & 0xFFFFFFFF;
    buffer[7] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
    checksum += (unsigned int) buffer[7];
    buffer[8] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
    checksum += (unsigned int) buffer[8];
    buffer[9] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
    checksum += (unsigned int) buffer[9];
    buffer[10] = (unsigned char)(tmpLen & 0x000000FF);
    checksum += (unsigned int) buffer[10];

    /** payload */
    for (j=0, i=11; j<ephemeris_length; j++, i++) {
      buffer[i] = (unsigned char) ((unsigned char *) ephemeris_buffer)[j];
      checksum += (unsigned int) buffer[i];
    }

    /** chksumHH, chksumHL, chksumLH, chksumLL */
    buffer[i]   = (unsigned char) ((checksum & 0xFF000000) >> 24);
    buffer[i+1] = (unsigned char) ((checksum & 0x00FF0000) >> 16);
    buffer[i+2] = (unsigned char) ((checksum & 0x0000FF00) >> 8);
    buffer[i+3] = (unsigned char) ((checksum & 0x000000FF));
  }

  pthread_mutex_unlock (&ephemeris_mutex);

  if (buffer) {
    pthread_mutex_lock (&sock_mutex);
    write (newsockfd, buffer, bufferlen);
    pthread_mutex_unlock (&sock_mutex);
  }

  return 0;
}


///////////////////////////
// commit_file
///////////////////////////
int commit_file (char *filename, int filetype) {

#ifdef _COMM_MODULE_DEBUG
  int debugIter;
#endif

  unsigned char *buffer;
  long long filelen;
  unsigned short checksum = 0;
  unsigned long tmpLen = 0;

  struct stat filestatus;
  off_t offset = 0;

  int fid;

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "commit_file\n");
#endif

  /**
   *  send file to socket
   */

  fid = open (filename, O_RDONLY);
  if (fid<0) return (-1);

  if (fstat (fid, &filestatus) < 0) return (-2);
  if (!S_ISREG (filestatus.st_mode)) return (-3);
  filelen = filestatus.st_size;

  if (filelen == 0) return (-1);
#ifdef _COMM_MODULE_DEBUG
  /*MN*/
  //fprintf(stdout, "commit_file - filelen=%d\n", filelen);
  fprintf(stdout, "commit_file - filelen=%lld\n", filelen);
  /**/
#endif

  // Send header

  if (!(buffer = malloc (15))) return (-4);
  // 15 is the length of msg without payload

  pthread_mutex_lock (&sock_mutex);

  /** soh */
  buffer[0] = (unsigned char) SOH;
  checksum += (unsigned int) buffer[0];

  /** cmd, cmdcpl */
  buffer[1] = (unsigned char) filetype;
  checksum += (unsigned int) buffer[1];
  buffer[2] = (unsigned char) (~(buffer[1]));
  checksum += (unsigned int) buffer[2];

  /** lenHHH, lenHHL, lenHLH, lenHLL, lenLHH, lenLHL, lenLLH, lenLLL */
  tmpLen = filelen >> 32;
#ifdef _COMM_MODULE_DEBUG
  /* MN */
  //fprintf(stdout, "tmpLen 1=%d\n", tmpLen);
  fprintf(stdout, "tmpLen 1=%lu\n", tmpLen);
  /**/
#endif
//  buffer[3] = (unsigned char)((filelen & 0xFF000000) >> 24);
  buffer[3] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
  checksum += (unsigned int) buffer[3];
  buffer[4] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
  checksum += (unsigned int) buffer[4];
  buffer[5] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
  checksum += (unsigned int) buffer[5];
  buffer[6] = (unsigned char)((tmpLen & 0x000000FF));
  checksum += (unsigned int) buffer[6];

  tmpLen = filelen & 0xFFFFFFFF;
#ifdef _COMM_MODULE_DEBUG
  /* MN */
  //fprintf(stdout, "tmpLen 2=%d\n", tmpLen);
  fprintf(stdout, "tmpLen 2=%lu\n", tmpLen);
  /**/
#endif
  buffer[7] = (unsigned char)((tmpLen & 0xFF000000) >> 24);
  checksum += (unsigned int) buffer[7];
  buffer[8] = (unsigned char)((tmpLen & 0x00FF0000) >> 16);
  checksum += (unsigned int) buffer[8];
  buffer[9] = (unsigned char)((tmpLen & 0x0000FF00) >> 8);
  checksum += (unsigned int) buffer[9];
  buffer[10] = (unsigned char)(tmpLen & 0x000000FF);
  checksum += (unsigned int) buffer[10];

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "commit_file - length=");
  for (debugIter=0; debugIter < 8; debugIter++) {
      printf("0x%02x ", buffer[3+debugIter]);
  }
  printf("\n");
#endif

  write (newsockfd, buffer, 11);

  /** Send file (payload) */
  sendfile (newsockfd, fid, &offset, filelen);
#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "file sent\n");
#endif

  /** Send a dummy checksum */
  checksum = 0;
  /*
  lseek (fid, 0, SEEK_SET); // reposition file pointer to the beginning
  for (i=0; i<filelen; i++) {
    read (fid, &onebyte, 1);
    checksum += (unsigned int) onebyte;
#ifdef _COMM_MODULE_DEBUG
    fprintf(stdout, "checksum computed: %f\n", ((float)i/filelen));
#endif
  }

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "checksum computed\n");
#endif
  */

  /** chksumHH, chksumHL, chksumLH, chksumLL */
  buffer[11]  = (unsigned char) ((checksum & 0xFF000000) >> 24);
  buffer[12]  = (unsigned char) ((checksum & 0x00FF0000) >> 16);
  buffer[13]  = (unsigned char) ((checksum & 0x0000FF00) >> 8);
  buffer[14] = (unsigned char) ((checksum & 0x000000FF));

  write (newsockfd, &(buffer[11]), 4);

#ifdef _COMM_MODULE_DEBUG
  fprintf(stdout, "checksum sent\n");
#endif

  close (fid);

  pthread_mutex_unlock (&sock_mutex);

  return 0;
}
