#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>

typedef void (*usb_cdc_rx_char_cb_t)(char ch);

void usb_cdc_init(void);
void usb_cdc_poll(void);
void usb_cdc_set_rx_callback(usb_cdc_rx_char_cb_t cb);

void usb_cdc_write_bytes(const char *buf, uint16_t len);
void usb_cdc_write_text(const char *s);
void usb_cdc_echo_char(char ch);

#endif