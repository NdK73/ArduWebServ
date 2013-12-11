ArduWebServ
===========

Webserver for Arduino + Ethernet shield
It reads network config and serves pages from MicroSD!

Pages (.htm and .css) can contain references to variables. For performance, other contents (like images) are served as-is.

POST and GET variables can be handled quite easily in user callbacks.

Code contains other useful snippets:
 - input debouncing
 - checking changed input state
 - searching between strings stored in program memory

The supplied code communicates via I2C with an old relay board by BND Communications (seems it doesn't exists any more... but any PCA95340-based interface should work).
