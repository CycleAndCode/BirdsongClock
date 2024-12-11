# BirdsongClock

This is a "Birdsong Clock" project - a clock designed and coded in order to play songs on scheduled times.

Main hardware:
* ESP8266 with integrated 0.96" OLED display module
* MP3-TF-16P mp3 player
* DS1307 RTC

Features:
* Display time from RTC with 1sec resolution
* Display temperature 
* Sync RTC over NTP (WiFi) on startup
* automatically take into account DST (daylight saving time) for central Europe
* play songs on full hours
* play special scheduled songs
* utilize power saving witch Light Sleep mode

Because MP3-TF-16P's "sleep mode" refused to work properly whatever i tried (which is also reported by other users), a DPDT relay was used in order to disconnect it completely when not used.

CAUTION: This repository doesn't include necessary mp3 files.
In order to just make this work, format the microSD as FAT32, put a folder named "01" and place inside files named "006.mp3" up to "021.mp3", which is what to play from 6AM to 9PM. 
Review the code for more options.
You could be also interested in adjusting your time zone, because time synced from NTC is UTC.

If you not want any time display, you can consider to use Deep Sleep and replace the mp3 player with an I2S decoder.
