#ifndef RKL_ICONS_H
#define RKL_ICONS_H

#include <Arduino.h>

extern const byte ico_accountlogin[];
extern const byte ico_ban[];
extern const byte ico_batteryempty[];
extern const byte ico_batteryfull[];
extern const byte ico_bell[];
extern const byte ico_calendar[];
extern const byte ico_cameraslr[];
extern const byte ico_clock[];
extern const byte ico_cloudy[];
extern const byte ico_dollar[];
extern const byte ico_envelopeclosed[];
extern const byte ico_eye[];
extern const byte ico_heart[];
extern const byte ico_home[];
extern const byte ico_locklocked[];
extern const byte ico_lockunlocked[];
extern const byte ico_rain[];
extern const byte ico_sun[];
extern const byte ico_warning[];

const byte* const icons[] = {
  ico_accountlogin,
  ico_ban,
  ico_batteryempty,
  ico_batteryfull,
  ico_bell,
  ico_calendar,
  ico_cameraslr,
  ico_clock,
  ico_cloudy,
  ico_dollar,
  ico_envelopeclosed,
  ico_eye,
  ico_heart,
  ico_home,
  ico_locklocked,
  ico_lockunlocked,
  ico_rain,
  ico_sun,
  ico_warning,
  NULL
};

#define NUM_ICONS ((sizeof(icons)/sizeof(icons[0])) - 1)

#endif
