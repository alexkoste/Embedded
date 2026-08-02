#include "driver/ledc.h"
#include "hal/ledc_types.h"
#include "soc/clk_tree_defs.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#define BUZZZER_PIN GPIO_NUM_18
#define DAC_CHANNEL LEDC_CHANNEL_0
#define PLAYER_TICK_MS 50

typedef struct
{
    uint16_t freq;
    uint16_t duration_ms;
} note_t;

static const note_t melody[] =
{
    {330, 500},
	{0, 50},
	{330, 500},
	{0, 50},
	{330, 500},
	{0, 50},
	{330, 500},
	{0, 50},
	{330, 500},
	{0, 50},
	{330, 500},
	{0, 50},
    {330, 200},
    {392, 200},
    {261, 200},
    {293, 200},
	{330, 200},
    {0,   1000},
   
};

typedef struct
{
    const note_t *melody;

    uint16_t melody_size;

    uint16_t index;

    uint16_t ticks_left;

    bool playing;

    int64_t last_tick;

} audio_player_t;


static void buzzer_play(uint16_t freq)
{
    if(freq == 0)
    {
        ledc_set_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0,
            0);

        ledc_update_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0);

        return;
    }

    ledc_set_freq(
        LEDC_LOW_SPEED_MODE,
        LEDC_TIMER_0,
        freq);

    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        512);

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0);
}

audio_player_t player;

void player_init(
    const note_t *melody,
    uint16_t size)
{
    player.melody = melody;
    player.melody_size = size;
    player.index = 0;
    player.playing = true;
    player.last_tick = esp_timer_get_time();

    buzzer_play(player.melody[0].freq);

    player.ticks_left =
        player.melody[0].duration_ms /
        PLAYER_TICK_MS;
}

void player_process(void)
{
    if(!player.playing)
        return;

    int64_t now = esp_timer_get_time();

    if(now - player.last_tick < PLAYER_TICK_MS * 1000)
        return;

    player.last_tick += PLAYER_TICK_MS * 1000;

    if(player.ticks_left)
        player.ticks_left--;

    if(player.ticks_left)
        return;

    player.index++;

    if(player.index >= player.melody_size)
    {
        player.playing = false;
        buzzer_play(0);
        return;
    }

    buzzer_play(player.melody[player.index].freq);

    player.ticks_left =
        player.melody[player.index].duration_ms /
        PLAYER_TICK_MS;
}


void app_main(void)
{
	ledc_timer_config_t timer_conf = {
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.timer_num = LEDC_TIMER_0,
		.duty_resolution = LEDC_TIMER_10_BIT,
		.freq_hz = 2000,
		.clk_cfg = LEDC_AUTO_CLK
	};
	
	ledc_timer_config(&timer_conf);
	
	ledc_channel_config_t channel_config = {
			.speed_mode = LEDC_LOW_SPEED_MODE,
			.channel = DAC_CHANNEL,
			.timer_sel = LEDC_TIMER_0,
			.intr_type = LEDC_INTR_DISABLE,
			.gpio_num = BUZZZER_PIN,
			.duty = 0,
			.hpoint = 0
		};
	
	ledc_channel_config(&channel_config);
	
	player_init(
	       melody,
	       sizeof(melody) / sizeof(note_t));
	bool isFinished = player.playing;
	   while(1)
	   {
	       player_process();
		   if (isFinished!=player.playing) {player.playing = true; player.index = 0;}
	   }
}
