/* FlatOut build: stand-ins for the NFSU2-only runtime modules that the shared
 * ones still call (widescreen.c, mprace/roam/horn/carsfx, the Online lobby,
 * the language pack, d3dx_hle.c's NFSU2 address table, photo mode, sms).
 * Each answers "not active" / "unchanged"; none touches guest memory. */
#include "cpu.h"
#include <stdlib.h>

/* widescreen.c: no aspect/FOV fix-ups for FlatOut yet */
float ws_ui_x(void){ return 1.f; }
float ws_ui_y(void){ return 1.f; }
float ws_persp_x(void){ return 1.f; }
float ws_persp_y(void){ return 1.f; }
void ws_set_mapping(float k){ (void)k; }
int ws_take_border_quad(void){ return 0; }
void ws_report(void){}
double ws_render_factor(void){ return 1.0; }
double ws_max_pixels(void){ const char *e = getenv("FO_MAXPIX"); return e && *e ? atof(e) : 0.0; }
int ws_fill_target(void){ return 0; }

/* NFSU2 game features */
int mprace_owns_input(void){ return 0; }
void roam_game_parked(int on){ (void)on; }
void sms_request(int id){ (void)id; }
void photo_input(int cmd, int a, int b){ (void)cmd; (void)a; (void)b; }
void online_lobby_read(uint32_t link, const uint8_t *p, int n){ (void)link; (void)p; (void)n; }
void olfilter_lobby_sent(uint32_t link, uint8_t *p, int n){ (void)link; (void)p; (void)n; }
void olfilter_lobby_mark(uint32_t link, uint8_t *p, int n){ (void)link; (void)p; (void)n; }
const char *language_registry(void){ return "English"; }
void horn_mix(float *acc, unsigned frames, unsigned rate){ (void)acc; (void)frames; (void)rate; }
void carsfx_mix(float *acc, unsigned frames, unsigned rate){ (void)acc; (void)frames; (void)rate; }
void headlights_vs_ramp(float s, float o){ (void)s; (void)o; }
void headlights_report(void){}

/* d3dx_hle.c / overrides.c (NFSU2 addresses): not installed */
int g_efx_stat;
uint32_t g_fx_epoch;
uint32_t g_fe_mgr;
int g_dx_in_pass;
void efx_stat_note(CPU *c, uint32_t addr){ (void)c; (void)addr; }
void d3dx_hle_report(void){}
void overrides_report(void){}
void guest_cpu_mark(void){}

/* autotest.c / status_web.c: NFSU2 script commands and counters */
int roam_cmd(CPU *c, const char *line, long *wait_start){ (void)c; (void)line; (void)wait_start; return 0; }
int career_is_cmd(const char *cmd){ (void)cmd; return 0; }
int career_cmd(CPU *c, const char *line, long *wait_start){ (void)c; (void)line; (void)wait_start; return 0; }
int career_fe_active(const char *name){ (void)name; return 0; }
int roam_exploring(void){ return 0; }
int roam_game_cars(void){ return 0; }
int roam_game_friends(void){ return 0; }
int sms_delivered(void){ return 0; }
int photo_script(const char *s){ (void)s; return 0; }
int headlights_set(const char *mode){ (void)mode; return 0; }
uint32_t autopilot_vehicle(void){ return 0; }
