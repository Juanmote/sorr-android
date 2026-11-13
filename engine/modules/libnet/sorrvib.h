/* Vibracion segun los sonidos que pide el juego. Ver sorrvib.c */
#ifndef SORR_VIB_H
#define SORR_VIB_H

void sorrvib_loaded(const void *chunk, const char *filename);
void sorrvib_unloaded(const void *chunk);
void sorrvib_played(const void *chunk);

#endif
