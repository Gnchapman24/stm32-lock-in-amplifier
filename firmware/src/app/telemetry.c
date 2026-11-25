/*
 * telemetry.c - see telemetry.h
 *
 * Changes from the earlier project: host mode (tlm_set_host_mode, the "mode"
 * command and the "#END" marker), and tlm_event/stream are off in host mode.
 */
#include "telemetry.h"
#include "serial_port.h"
#include "stm32g4xx.h"          /* SystemCoreClock, for the cycles -> ns conversion */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TLM_PROMPT            "> "
#define TLM_MAX_ARGS          8
#define TLM_STATUS_MIN_FREE   256u    /* skip a status report if TX ring is this full  */

/* Input gate: a new byte is read from the RX ring only while the TX ring has at
 * least this much free space. It must be above the largest single reply (help
 * is ~530 bytes; console adds CRs) and below SERIAL_TX_BUF_SIZE, or the console
 * would stop accepting input for good. */
#define TLM_RX_GATE_FREE      1024u
_Static_assert(TLM_RX_GATE_FREE < SERIAL_TX_BUF_SIZE, "input gate must fit in the TX ring");
_Static_assert(TLM_RX_GATE_FREE >= 768u, "input gate must cover the largest reply");

/* Host-mode marker: a reply lost a message to a full TX ring. Sent before #END. */
#define TLM_TRUNC_MARK        "#TRUNC\n"
#define TLM_PERIOD_MIN_MS     10u
#define TLM_PERIOD_MAX_MS     60000u
#define ARRAY_LEN(a)          (sizeof(a) / sizeof((a)[0]))

static struct {
    uint32_t (*now_ms)(void);

    const tlm_cmd_t *cmds;
    size_t           ncmds;

    tlm_status_fn status_cb;
    bool          stream_on;
    uint32_t      period_ms;
    uint32_t      next_due_ms;

    char     line[TLM_LINE_MAX];
    size_t   len;
    bool     overflow;
    uint8_t  prev;               /* previous received char (CR/LF pairing)   */
    uint8_t  esc;                /* 0 = normal, 1 = got ESC, 2 = in CSI/SS3  */
    bool     prompt_visible;     /* prompt + partial line currently on screen */
    bool     cmd_done;           /* a command ran during this poll (one per pass) */
    bool     host_mode;          /* machine protocol: no echo/redraw/events  */
    bool     stream_before_host; /* stream setting to restore on "mode console" */

    uint32_t dropped_msgs;       /* whole messages dropped: TX ring was full */
    uint32_t truncated_msgs;     /* messages longer than TLM_MSG_MAX         */
    uint32_t skipped_status;     /* status reports skipped (back-pressure)   */
    uint32_t suppressed_events;  /* tlm_event() calls ignored in host mode   */
} s;

/* ------------------------------------------------------------------------ *
 *  Low-level output
 * ------------------------------------------------------------------------ */

/* Queue raw bytes if they ALL fit, otherwise queue nothing. */
static bool emit_raw(const char *p, size_t n)
{
    if (n == 0u) { return true; }
    if (serial_tx_free() < n) { return false; }
    serial_write((const uint8_t *)p, n);
    return true;
}

/* Never waits. A message that doesn't fit is dropped and counted (dropped_msgs).
 * The input gate in tlm_poll() keeps this from happening to a well-behaved host:
 * a new command is read only when the TX ring has room for any reply. */
static bool have_space(size_t need)
{
    return serial_tx_free() >= need;
}

/* Send text, expanding bare '\n' to "\r\n" (console mode only).
 * All-or-nothing per message. */
static int emit_text(const char *text, size_t len)
{
    if (s.host_mode) {
        if (!have_space(len)) {
            s.dropped_msgs++;
            return 0;
        }
        serial_write((const uint8_t *)text, len);
        return (int)len;
    }

    size_t out_len = len;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n' && (i == 0u || text[i - 1u] != '\r')) { out_len++; }
    }
    if (!have_space(out_len)) {
        s.dropped_msgs++;
        return 0;
    }

    size_t seg = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n' && (i == 0u || text[i - 1u] != '\r')) {
            serial_write((const uint8_t *)&text[seg], i - seg);
            serial_write((const uint8_t *)"\r\n", 2u);
            seg = i + 1u;
        }
    }
    serial_write((const uint8_t *)&text[seg], len - seg);
    return (int)out_len;
}

static int tlm_vprintf(const char *fmt, va_list ap)
{
    char tmp[TLM_MSG_MAX];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    if (n < 0) { return -1; }

    size_t len = (size_t)n;
    if (len >= sizeof tmp) {
        len = sizeof tmp - 1u;
        s.truncated_msgs++;
    }
    return emit_text(tmp, len);
}

int tlm_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = tlm_vprintf(fmt, ap);
    va_end(ap);
    return r;
}

/* ---- asynchronous output while the user may be typing ------------------ */

static void print_prompt(void)
{
    if (s.host_mode) { return; }
    emit_raw(TLM_PROMPT, sizeof(TLM_PROMPT) - 1u);
    s.prompt_visible = true;
}

static void async_begin(void)
{
    if (s.prompt_visible) {
        emit_raw("\r\x1b[2K", 5u);                 /* CR + erase whole line (VT100) */
    }
}

static void async_end(void)
{
    if (s.prompt_visible) {
        emit_raw(TLM_PROMPT, sizeof(TLM_PROMPT) - 1u);
        emit_raw(s.line, s.len);                   /* restore half-typed command   */
    }
}

int tlm_event(const char *fmt, ...)
{
    /* Unsolicited output would land in the middle of a host's reply or a
     * binary frame, so host mode drops events (and counts them). */
    if (s.host_mode) {
        s.suppressed_events++;
        return 0;
    }

    char tmp[TLM_MSG_MAX];
    uint32_t t = (s.now_ms != NULL) ? s.now_ms() : 0u;

    int p = snprintf(tmp, sizeof tmp, "[%lu.%03lu] ",
                     (unsigned long)(t / 1000u), (unsigned long)(t % 1000u));
    if (p < 0 || (size_t)p >= sizeof tmp - 2u) { return -1; }

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp + p, sizeof tmp - (size_t)p - 1u, fmt, ap);  /* -1: room for '\n' */
    va_end(ap);
    if (n < 0) { return -1; }

    size_t len = (size_t)p + (size_t)n;
    if (len >= sizeof tmp - 1u) {
        len = sizeof tmp - 2u;
        s.truncated_msgs++;
    }
    tmp[len++] = '\n';

    async_begin();
    int r = emit_text(tmp, len);
    async_end();
    return r;
}

/* ------------------------------------------------------------------------ *
 *  Helpers
 * ------------------------------------------------------------------------ */

bool tlm_parse_u32(const char *str, uint32_t *out)
{
    if (str == NULL || *str == '\0' || *str == '-') { return false; }
    char *end;
    errno = 0;
    unsigned long v = strtoul(str, &end, 0);       /* base 0: decimal, 0x.., 0.. octal */
    if (end == str || *end != '\0' || errno == ERANGE) { return false; }
    *out = (uint32_t)v;
    return true;
}

static bool name_eq(const char *a, const char *b)
{
    for (;; a++, b++) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') { ca = (char)(ca + ('a' - 'A')); }
        if (cb >= 'A' && cb <= 'Z') { cb = (char)(cb + ('a' - 'A')); }
        if (ca != cb)  { return false; }
        if (ca == '\0') { return true; }
    }
}

/* ------------------------------------------------------------------------ *
 *  Built-in commands
 * ------------------------------------------------------------------------ */

static void cmd_help(int argc, char **argv);
static void cmd_status(int argc, char **argv);
static void cmd_stream(int argc, char **argv);
static void cmd_period(int argc, char **argv);
static void cmd_stats(int argc, char **argv);
static void cmd_mode(int argc, char **argv);

static const tlm_cmd_t k_builtin[] = {
    { "help",   "list commands",                        cmd_help   },
    { "status", "print one status report now",          cmd_status },
    { "stream", "stream [on|off]  periodic status",     cmd_stream },
    { "period", "period <ms>  status interval",         cmd_period },
    { "stats",  "serial/telemetry error counters",      cmd_stats  },
    { "mode",   "mode [console|host]  terminal or script protocol", cmd_mode },
};

static void print_cmds(const tlm_cmd_t *t, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        tlm_printf("  %-8s %s\n", t[i].name, t[i].help ? t[i].help : "");
    }
}

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    tlm_printf("Commands:\n");
    print_cmds(k_builtin, ARRAY_LEN(k_builtin));
    if (s.cmds != NULL) { print_cmds(s.cmds, s.ncmds); }
}

static void cmd_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (s.status_cb != NULL) { s.status_cb(s.now_ms()); }
    else                     { tlm_printf("no status callback registered\n"); }
}

static void cmd_stream(int argc, char **argv)
{
    if (argc >= 2) {
        if      (name_eq(argv[1], "on"))  { tlm_stream(true);  }
        else if (name_eq(argv[1], "off")) { tlm_stream(false); }
        else { tlm_printf("usage: stream [on|off]\n"); return; }
    }
    tlm_printf("stream %s, period %lu ms\n",
               s.stream_on ? "on" : "off", (unsigned long)s.period_ms);
}

static void cmd_period(int argc, char **argv)
{
    uint32_t ms;
    if (argc != 2 || !tlm_parse_u32(argv[1], &ms)) {
        tlm_printf("usage: period <%lu..%lu ms>\n",
                   (unsigned long)TLM_PERIOD_MIN_MS, (unsigned long)TLM_PERIOD_MAX_MS);
        return;
    }
    if (ms < TLM_PERIOD_MIN_MS) { ms = TLM_PERIOD_MIN_MS; }
    if (ms > TLM_PERIOD_MAX_MS) { ms = TLM_PERIOD_MAX_MS; }
    s.period_ms = ms;
    tlm_printf("period = %lu ms\n", (unsigned long)ms);
}

static void cmd_stats(int argc, char **argv)
{
    if (argc >= 2 && name_eq(argv[1], "reset")) {
        serial_reset_isr_max();
        tlm_printf("LPUART1 ISR max reset\n");
    }
    serial_stats_t st;
    serial_get_stats(&st);
    tlm_printf("rx=%lu tx=%lu bytes\n",
               (unsigned long)st.rx_bytes, (unsigned long)st.tx_bytes);
    tlm_printf("uart: overrun=%lu framing=%lu noise=%lu parity=%lu\n",
               (unsigned long)st.uart_overrun, (unsigned long)st.framing_err,
               (unsigned long)st.noise_err,    (unsigned long)st.parity_err);
    tlm_printf("ring drops: rx=%lu tx=%lu\n",
               (unsigned long)st.rx_ring_dropped, (unsigned long)st.tx_ring_dropped);
    tlm_printf("LPUART1 ISR: calls=%lu  longest=%lu cycles (%lu ns)\n",
               (unsigned long)st.isr_calls, (unsigned long)st.isr_max_cyc,
               (unsigned long)(st.isr_max_cyc * 1000u / (SystemCoreClock / 1000000u)));
    tlm_printf("telemetry: dropped_msgs=%lu truncated=%lu status_skipped=%lu events_suppressed=%lu\n",
               (unsigned long)s.dropped_msgs, (unsigned long)s.truncated_msgs,
               (unsigned long)s.skipped_status, (unsigned long)s.suppressed_events);
}

static void cmd_mode(int argc, char **argv)
{
    if (argc >= 2) {
        if      (name_eq(argv[1], "host"))    { tlm_set_host_mode(true);  }
        else if (name_eq(argv[1], "console")) { tlm_set_host_mode(false); }
        else { tlm_printf("usage: mode [console|host]\n"); return; }
    }
    tlm_printf("mode %s\n", s.host_mode ? "host" : "console");
}

/* ------------------------------------------------------------------------ *
 *  Command dispatch
 * ------------------------------------------------------------------------ */

static const tlm_cmd_t *find_cmd(const tlm_cmd_t *t, size_t n, const char *name)
{
    for (size_t i = 0; t != NULL && i < n; i++) {
        if (name_eq(t[i].name, name)) { return &t[i]; }
    }
    return NULL;
}

static void dispatch(char *line)
{
    char *argv[TLM_MAX_ARGS];
    int   argc = 0;
    char *p = line;

    while (*p != '\0') {                      /* split on spaces, in place */
        while (*p == ' ') { p++; }
        if (*p == '\0') { break; }
        if (argc == TLM_MAX_ARGS) {
            tlm_printf("error: too many arguments (max %d)\n", TLM_MAX_ARGS);
            return;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ') { p++; }
        if (*p != '\0') { *p++ = '\0'; }
    }
    if (argc == 0) { return; }

    const tlm_cmd_t *c = find_cmd(k_builtin, ARRAY_LEN(k_builtin), argv[0]);
    if (c == NULL) { c = find_cmd(s.cmds, s.ncmds, argv[0]); }

    if (c != NULL && c->fn != NULL) { c->fn(argc, argv); }
    else { tlm_printf("unknown command '%s' (try 'help')\n", argv[0]); }
}

static void run_line(void)
{
    s.line[s.len] = '\0';
    if (!s.host_mode) {
        emit_raw("\r\n", 2u);                 /* move below the echoed input */
    }
    s.prompt_visible = false;
    bool was_host = s.host_mode;
    uint32_t dropped_before = s.dropped_msgs;

    if (s.overflow) {
        tlm_printf("error: line too long (max %d characters)\n", TLM_LINE_MAX - 1);
    } else if (s.len > 0u) {
        dispatch(s.line);
    }

    /* Host mode: mark the end of this command's output, even if it printed
     * nothing, so the script never has to guess with a timeout. Either side
     * of a mode change counts, so "mode host" and "mode console" both end
     * with the marker. If a message of this reply was dropped, say so first,
     * so the script knows the reply is incomplete rather than silently short. */
    if (was_host || s.host_mode) {
        const size_t t = sizeof(TLM_TRUNC_MARK) - 1u;
        const size_t n = sizeof(TLM_HOST_END) - 1u;
        if (s.dropped_msgs != dropped_before && have_space(t + n)) {
            (void)emit_raw(TLM_TRUNC_MARK, t);
        }
        if (have_space(n)) { (void)emit_raw(TLM_HOST_END, n); }
        else               { s.dropped_msgs++; }
    }

    s.cmd_done = true;                        /* tlm_poll() stops after this line */
    s.len = 0;
    s.overflow = false;
    print_prompt();
}

/* ------------------------------------------------------------------------ *
 *  Line editor
 * ------------------------------------------------------------------------ */

static void rx_char(uint8_t c)
{
    /* Swallow terminal escape sequences (arrow keys, Home/End, F-keys ...) */
    if (s.esc == 1u) { s.esc = (c == '[' || c == 'O') ? 2u : 0u; return; }
    if (s.esc == 2u) { if (c >= 0x40u && c <= 0x7Eu) { s.esc = 0u; } return; }
    if (c == 0x1Bu)  { s.esc = 1u; return; }

    uint8_t prev = s.prev;
    s.prev = c;

    if (c == '\r' || c == '\n') {
        if (c == '\n' && prev == '\r') { return; }       /* second half of CRLF */
        run_line();
    } else if (c >= 0x20u && c < 0x7Fu) {                /* printable */
        if (s.len < TLM_LINE_MAX - 1u) {
            s.line[s.len++] = (char)c;
            if (!s.host_mode) { emit_raw((const char *)&c, 1u); }   /* echo */
        } else {
            s.overflow = true;                           /* reported on Enter */
        }
    } else if (c == 0x08u || c == 0x7Fu) {               /* backspace / DEL   */
        if (s.len > 0u) {
            s.len--;
            if (!s.host_mode) { emit_raw("\b \b", 3u); }
        }
    } else if (c == 0x03u) {                             /* Ctrl-C: abandon line */
        s.len = 0; s.overflow = false;
        if (!s.host_mode) { emit_raw("^C\r\n", 4u); }
        print_prompt();
    } else if (c == 0x15u) {                             /* Ctrl-U: clear line   */
        s.len = 0; s.overflow = false;
        if (!s.host_mode) { emit_raw("\r\x1b[2K", 5u); }
        print_prompt();
    }
    /* anything else (TAB, other control codes) is ignored */
}

/* ------------------------------------------------------------------------ *
 *  Public API
 * ------------------------------------------------------------------------ */

void tlm_init(uint32_t baud, uint32_t (*now_ms)(void), const char *banner)
{
    memset(&s, 0, sizeof s);
    s.now_ms    = now_ms;
    s.period_ms = 1000u;

    serial_init(baud);

    if (banner != NULL) { tlm_printf("\n%s\n", banner); }
    tlm_printf("Type 'help' for commands.\n");
    print_prompt();
}

void tlm_set_commands(const tlm_cmd_t *table, size_t count)
{
    s.cmds  = table;
    s.ncmds = count;
}

void tlm_set_status_cb(tlm_status_fn cb, uint32_t period_ms)
{
    s.status_cb = cb;
    if (period_ms < TLM_PERIOD_MIN_MS) { period_ms = TLM_PERIOD_MIN_MS; }
    if (period_ms > TLM_PERIOD_MAX_MS) { period_ms = TLM_PERIOD_MAX_MS; }
    s.period_ms = period_ms;
    tlm_stream(true);
}

void tlm_stream(bool on)
{
    s.stream_on = on;
    if (on && s.now_ms != NULL) { s.next_due_ms = s.now_ms(); }   /* first report at once */
}

void tlm_set_host_mode(bool on)
{
    if (on == s.host_mode) { return; }
    if (on) {
        s.stream_before_host = s.stream_on;
        tlm_stream(false);
        s.host_mode      = true;
        s.prompt_visible = false;        /* nothing to erase or redraw from now on */
    } else {
        s.host_mode = false;
        tlm_stream(s.stream_before_host);
        /* run_line() prints the prompt after this command finishes. */
    }
}

bool tlm_host_mode(void)
{
    return s.host_mode;
}

void tlm_poll(void)
{
    /* 1. Input. Bounded per pass, in two ways:
     *    - at most one command runs per pass (its formatting is the pass cost);
     *    - bytes are read only while the TX ring can take a reply (input gate).
     *    Unread bytes wait in the RX ring. If that overflows, rx_ring_dropped
     *    counts it. Nothing here waits for the wire. */
    s.cmd_done = false;
    for (unsigned budget = 64u; budget != 0u && !s.cmd_done; budget--) {
        if (serial_tx_free() < TLM_RX_GATE_FREE) { break; }
        int c = serial_read_byte();
        if (c < 0) { break; }
        rx_char((uint8_t)c);
    }

    /* 2. Periodic status stream. */
    if (s.stream_on && !s.host_mode && s.status_cb != NULL && s.now_ms != NULL) {
        uint32_t now = s.now_ms();
        if ((int32_t)(now - s.next_due_ms) >= 0) {
            /* Drift-free schedule; resynchronise if we fell more than a period behind. */
            s.next_due_ms += s.period_ms;
            if ((int32_t)(now - s.next_due_ms) >= 0) { s.next_due_ms = now + s.period_ms; }

            if (serial_tx_free() < TLM_STATUS_MIN_FREE) {
                s.skipped_status++;               /* back-pressure: skip, don't garble */
            } else {
                async_begin();
                s.status_cb(now);
                async_end();
            }
        }
    }
}
