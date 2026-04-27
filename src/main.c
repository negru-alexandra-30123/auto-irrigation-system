/*
 *  Sistem automat de irigare a plantelor
 *  MCU  : ATmega32 @  16 MHz 
 */

#define F_CPU 16000000UL //adaugat ininte de #include <util/delay.h> pentru a functiona
      
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>


/* Senzor umiditate sol — ADC0 (PA0) */
#define SOIL_ADC_CHANNEL     0

/* Releu pompa — PB0 (activ HIGH) */
#define PUMP_PORT            PORTB
#define PUMP_DDR             DDRB
#define PUMP_PIN             PB0

/* Senzor nivel apa — PD2 (activ LOW = rezervor gol) */
#define WATER_PORT           PORTD
#define WATER_DDR            DDRD
#define WATER_PIN_REG        PIND
#define WATER_PIN            PD2

/* LED de stare / avertizare — PB1 */
#define LED_PORT             PORTB
#define LED_DDR              DDRB
#define LED_PIN              PB1


#define INTERVAL_CITIRE_MS       15000UL
#define DURATA_POMPARE_MS        5000UL
#define PERIOADA_ABSORBTIE_MS    10000UL
/* (0–100 %) */
#define PRAG_UMIDITATE_MIN       30
#define MAX_CICLURI_FARA_PROGRES 5

/* Rezolutie ADC: 10 biti 0–1023; Vref = AVCC */
#define ADC_MAX                  1023


typedef enum {
    STARE_CITIRE,       
    STARE_POMPARE,       
    STARE_ABSORBTIE,     
    STARE_EROARE         
} StareSistem;

volatile uint32_t ms_ticks = 0;   /* Contor milisecunde (Timer1)   */

static StareSistem stare_curenta  = STARE_CITIRE;
static uint32_t    timestamp_start = 0;   /* Momentul intrarii în starea curenta */
static uint8_t     cicluri_fara_progres = 0;
static uint8_t     umiditate_anterioara  = 0;

ISR(TIMER1_COMPA_vect) {
    ms_ticks++;
}

/**
 * o întrerupere la fiecare 1 ms (F_CPU / 64 / 1000 - 1).
 */
static void timer1_init(void)
{
    /* Mod CTC, prescaler = 64 */
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

    /* OCR1A = F_CPU / prescaler / 1000 - 1 */
    OCR1A = (F_CPU / 64UL / 1000UL) - 1;

    /* Activeaza întreruperea de comparare A */
    TIMSK |= (1 << OCIE1A);
}

/** Configureaza modulul ADC cu Vref = AVCC, prescaler = 64. */
static void adc_init(void)
{
    ADMUX  = (1 << REFS0);                          /* Vref = AVCC   */
    ADCSRA = (1 << ADEN)                            /* Enable ADC    */
           | (1 << ADPS2) | (1 << ADPS1);          /* Prescaler /64 */
}

/** Configureaz? direc?ia pinilor GPIO. */
static void gpio_init(void)
{
    PUMP_DDR |= (1 << PUMP_PIN);
    LED_DDR  |= (1 << LED_PIN);

    WATER_DDR  &= ~(1 << WATER_PIN);
    WATER_PORT |=  (1 << WATER_PIN);   /* activeaza pull-up */
}


static uint32_t millis(void)
{
    uint32_t t;
    cli();          /* Dezactiveaza întreruperile temporar */
    t = ms_ticks;
    sei();
    return t;
}

static uint16_t adc_citeste(uint8_t canal)
{
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);

    ADCSRA |= (1 << ADSC);                  
    while (ADCSRA & (1 << ADSC));           

    return ADC;
}

static uint8_t adc_la_procente(uint16_t valoare_adc)
{
    const uint16_t ADC_USCAT = 900;   
    const uint16_t ADC_UD    = 300;   

    if (valoare_adc >= ADC_USCAT) return 0;
    if (valoare_adc <= ADC_UD)    return 100;

    return (uint8_t)(((ADC_USCAT - valoare_adc) * 100UL)
                     / (ADC_USCAT - ADC_UD));
}

static uint8_t citireSenzorUmiditate(void)
{
    uint16_t brut = adc_citeste(SOIL_ADC_CHANNEL);
    return adc_la_procente(brut);
}

static bool rezervorAreApa(void)
{
    return !(WATER_PIN_REG & (1 << WATER_PIN));
}


static void pornestePompa(void)
{
    PUMP_PORT |= (1 << PUMP_PIN);
}

static void oprestePompa(void)
{
    PUMP_PORT &= ~(1 << PUMP_PIN);
}

static void led_on(void)
{
    LED_PORT |= (1 << LED_PIN);
}

static void led_off(void)
{
    LED_PORT &= ~(1 << LED_PIN);
}

static void led_clipeste(uint8_t numar, uint16_t perioada_ms)
{
    for (uint8_t i = 0; i < numar; i++) {
        led_on();
        _delay_ms(100);     /* puls scurt vizibil */
        led_off();
        /* Pauza restant? — nu dep??im bugetul de timp pe bucla principal? */
        uint16_t pauza = (perioada_ms / 2 > 100) ? (perioada_ms / 2 - 100) : 0;
        for (uint16_t j = 0; j < pauza; j++) _delay_ms(1);
    }
}


static void intraInEroare(void)
{
    oprestePompa();
    stare_curenta = STARE_EROARE;
}

static void proceseazaSistem(void)
{
    uint32_t acum = millis();

    switch (stare_curenta) {

    case STARE_CITIRE:
    {
        if (acum - timestamp_start < INTERVAL_CITIRE_MS) {
            led_off();
            return;
        }

        uint8_t umiditate = citireSenzorUmiditate();

        if (umiditate >= PRAG_UMIDITATE_MIN) {
            cicluri_fara_progres = 0;
            umiditate_anterioara  = umiditate;
            timestamp_start       = acum;  
            led_off();
        } else {
            if (!rezervorAreApa()) {
                led_clipeste(3, 500);   
                timestamp_start = acum;     
                return;
            }

            if (umiditate <= umiditate_anterioara && umiditate_anterioara != 0) {
                cicluri_fara_progres++;
            } else {
                cicluri_fara_progres = 0;
            }

            if (cicluri_fara_progres >= MAX_CICLURI_FARA_PROGRES) {
                intraInEroare();
                return;
            }

            umiditate_anterioara = umiditate;

            pornestePompa();
            led_on();
            stare_curenta  = STARE_POMPARE;
            timestamp_start = acum;
        }
        break;
    }

    case STARE_POMPARE:
    {
        if (!rezervorAreApa()) {
            oprestePompa();
            led_clipeste(5, 300);
            stare_curenta   = STARE_CITIRE;
            timestamp_start = acum;
            return;
        }

        if (acum - timestamp_start >= DURATA_POMPARE_MS) {
            oprestePompa();
            led_off();
            stare_curenta   = STARE_ABSORBTIE;
            timestamp_start = acum;
        }
        break;
    }
    case STARE_ABSORBTIE:
    {
 
        if ((acum / 500) % 2 == 0) {
            led_on();
        } else {
            led_off();
        }

        if (acum - timestamp_start >= PERIOADA_ABSORBTIE_MS) {
            stare_curenta   = STARE_CITIRE;
            timestamp_start = acum;
            led_off();
        }
        break;
    }

    case STARE_EROARE:
    {
        oprestePompa();  

        if ((acum / 200) % 10 < 4) {
            led_on();
        } else {
            led_off();
        }
        break;
    }

    } 
}

int main(void)
{
    gpio_init();
    adc_init();
    timer1_init();
    sei();   /* Activeaza întreruperile globale */

    timestamp_start = millis() - INTERVAL_CITIRE_MS + 10000UL;

    while (1) {
        proceseazaSistem();
    }

    return 0;
}