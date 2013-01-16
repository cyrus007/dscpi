
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <wiringPi.h>
#include <softPwm.h>

#define Q1PIN	11	// physical pin 26
#define Q2PIN	12	// physical pin 19
#define Q3PIN	13	// physical pin 21
#define Q4PIN	14	// physical pin 23
#define LATCHPIN	10	// physical pin 24
#define RINGPIN	7	// physical pin 7

#define TONE_PIN	1	// physical pin 12
#define DIALTONE	400	// dial tone is 400hz
#define DTONE_RANGE	25
#define DTONE_VALUE	12
#define KISSTONE	1400	// kiss-off tone is 1400hz
#define KTONE_RANGE	7
#define KTONE_VALUE	4

static volatile char dtmf_val = 0 ;
static volatile char code[16] ;

/*
 * intrDTMF: routine to decode DTMF tone values
 *********************************************************************************
 */

void intrDTMF (void)
{
  int q1, q2, q3, q4;
  uint8_t reg_val;

  q1 = digitalRead(Q1PIN);
  q2 = digitalRead(Q2PIN);
  q3 = digitalRead(Q3PIN);
  q4 = digitalRead(Q4PIN);
  reg_val = (q4 << 3) | (q3 << 2) | (q2 << 1) | q1;
  switch (dtmf_val) {
    case 0:
      dtmf_val = 'D';
      break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      dtmf_val = reg_val + 48;
      break;
    case 10:
      dtmf_val = '0';
      break;
    case 11:
      dtmf_val = '*';
      break;
    case 12:
      dtmf_val = '#';
      break;
    case 13:
      dtmf_val = 'A';
      break;
    case 14:
      dtmf_val = 'B';
      break;
    case 15:
      dtmf_val = 'C';
      break;
    default:
      dtmf_val = 'Z';
      break;
  }
}

int genTeltone (int tone)
{
  if (tone == DIALTONE)
    softPwmCreate (TONE_PIN, DTONE_VALUE, DTONE_RANGE) ;
  else if (tone == KISSTONE)
    softPwmCreate (TONE_PIN, KTONE_VALUE, KTONE_RANGE) ;
  else {
    fprintf (stdout, "Wrong tone code - %i\n", tone) ;
    return 1 ;
  }
  return 0;
}

int main ()
{
  int i;

  if (wiringPiSetup () == -1)
  {
    fprintf (stdout, "oops: %s\n", strerror (errno)) ;
    return 1 ;
  }

  pinMode(Q1PIN, INPUT);
  pinMode(Q2PIN, INPUT);
  pinMode(Q3PIN, INPUT);
  pinMode(Q4PIN, INPUT);
  pinMode(LATCHPIN, INPUT);
  pinMode(RINGPIN, INPUT);

  if (wiringPiISR (LATCHPIN, INT_EDGE_RISING, &intrDTMF) < 0) {
    fprintf (stderr, "Unable to setup ISR: %s\n", strerror(errno)) ;
    return 1 ;
  }

  printf ("Starting the main alarm routine ... ") ; fflush (stdout) ;

  for (;;) {
    genTeltone (DIALTONE);	// phone can go off-hook only if there is dial-tone

    while (digitalRead(RINGPIN) == HIGH) {	// phone is off-hook
      // at this time panel calls central stn number and once it is off-hook
      delay (1000);				// and waits for 1 sec
      // at this time panel sends Contact-ID handshake which is
      // 100mSec of 1400Hz pure-tone + 100mSec of silence + 100mSec of 2300Hz pure-tone
      // 250mSec after that it sends Contact-ID message of 16 DTMF digits
      delay (550);
      // each digit is 50mSec long separated by 50mSec silence
      // last digit is checksum digit
      i = 16;
      while (i > 0) {
        code[16 - i] = dtmf_val;
        delay (50);
        i -= 1;
      }
      // after the central stn succesfully receives the msg it sends a kiss-off msg
      // which is 800mSec of 1400Hz pure-tone
      genTeltone (KISSTONE);
      delay (800);
      // if panel does not receive the kiss-off tone then it retransmits the msg
    }
  }

  return 0;
}
