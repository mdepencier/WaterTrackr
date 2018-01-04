# WaterTrackr
The project allows people to measure the aspect of their carbon footprint related to water consumption. As well as allowing users to monitor their water consumption and review their statistics over a time interval using stat files. This device was created as our final project for ECE 150.

# What does this project do?
Our project monitors the water being used by three devices. For testing, we pre programmed these to 3 devices found in the New Residence Building, University of Waterloo Place: 

* Bathroom sink on medium flow
* Bathroom sink on full flow
* Shower on full flow

Our project requires the users to toggle a switch on/off for each device they use to tell our program that water is running. Each device corresponds to its own GPIO pin on our power dock. Instead of using a seven segment display, the data is output to the console every minute and statistics files every minute minutes. It also logs errors to our ‘errors.log’ file. 

The format for the statistic files is ‘statsN.stat’ where N is the minute of the current session that statistics were calculated from (ie. stats1.stat for the first minute).

Default flow rates are programmed into our program, but the user can specify up to 3 arguments:

* Flow rate of bathroom sink on medium (Switch 1)
* Bathroom sink running full flow (Switch 2)
* Shower on full flow (Switch 3)

These values are in litres per second. The user can measure the time it takes for their own faucet/shower to output a litre of water, and then calculate its flow rate by taking the inverse of that value. This process allows customization because users can configure the project to be compatible with any household. Additionally, we have a customizable water limit that the user can set (called ‘quota’ in the software), which will cause an led to turn on when the total litres of water begins to approach the limit.

# Contributors
* Matthew Depencier
* Damian Reiter
* Jay Shrivastava
* Jason Xian
