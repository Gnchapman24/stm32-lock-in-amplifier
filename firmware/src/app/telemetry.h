/*
 * telemetry.h - terminal telemetry + command console on top of serial_port
 *
 *  Output side
 *    tlm_printf()  printf-style text; '\n' is sent as "\r\n" automatically.
 *    tlm_event()   same, but timestamped "[12.345] ..." and safe to emit while
 *                  the user is half-way through typing a command (the input
 *                  line is erased, the event printed, then the line redrawn).
 *    status callback + stream: a periodic status report you provide.
 *
 *  Input side
 *    A small line editor (echo, backspace, Ctrl-C / Ctrl-U, arrow keys are
 *    swallowed) feeds a command table.  Built-ins: help, status, stream,
 *    period, stats.  Add your own with tlm_set_commands().
 *
 *  Two modes (decision D34):
 *    console  for a person in a terminal: echo, line editing, '\n' sent as
 *             "\r\n", timestamped events and the periodic status stream.
 *    host     for a script: none of the above. Input isn't echoed, output is
 *             sent exactly as written, events and the stream are off, and the
 *             output of every command line ends with a line "#END". The host
 *             script sends "mode host" first, then reads replies up to "#END".
 *    Binary data never goes through tlm_printf; see capture (later stage).
 *
 *  Threading: everything here is for the MAIN LOOP only (not for ISRs).
 *
 *  Reused from the earlier telemetry project (D18); host mode is new.
 *
 *  PuTTY settings (Connection > Serial, Terminal):
 *      Speed 921600, 8 data bits, 1 stop bit, Parity none, Flow control none
 *      Local echo = Force off, Local line editing = Force off
 *      (Enter sends CR - the code accepts CR, LF or CRLF.)
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef TLM_LINE_MAX
#define TLM_LINE_MAX    64      /* longest command line, incl. terminator   */
#endif
#ifndef TLM_MSG_MAX
#define TLM_MSG_MAX     160     /* longest single formatted message         */
#endif

/* Command handler: argv[0] is the command name, as typed. */
typedef void (*tlm_cmd_fn)(int argc, char **argv);

typedef struct {
    const char *name;           /* matched case-insensitively               */
    const char *help;           /* one-line description shown by 'help'     */
    tlm_cmd_fn  fn;
} tlm_cmd_t;

/* Periodic status report: print your values with tlm_printf(). */
typedef void (*tlm_status_fn)(uint32_t now_ms);

/* Marks the end of each command's output in host mode. If a message of that
 * reply was dropped (TX ring full), "#TRUNC" is sent just before "#END". */
#define TLM_HOST_END    "#END\n"

/* now_ms: millisecond tick source, e.g. millis().  banner may be NULL. */
void tlm_init(uint32_t baud, uint32_t (*now_ms)(void), const char *banner);

/* Call every main-loop iteration.  Never blocks. */
void tlm_poll(void);

void tlm_set_commands(const tlm_cmd_t *table, size_t count);
void tlm_set_status_cb(tlm_status_fn cb, uint32_t period_ms);  /* stream starts OFF */
void tlm_stream(bool on);

/* Console (false) or host (true) mode; see above. Also the "mode" command. */
void tlm_set_host_mode(bool on);
bool tlm_host_mode(void);

int  tlm_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int  tlm_event (const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Decimal or 0x-hex.  Returns false on junk, a leading '-', or overflow. */
bool tlm_parse_u32(const char *s, uint32_t *out);

#endif /* TELEMETRY_H */
