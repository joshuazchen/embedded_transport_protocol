Embedded Transport Protocol
===========================

Transport Protocol for Embedded System

Introducation
=============

This is a compact protocol which working in half-duplex mode for reliable data
transmission. It has some useful features, such as auto resend, variable-length
data part. It was designed for embedded control system.

There are two editions, one for single master with single slave, another for
single master multiple slaves.

Features
========

* Half-duplex mode. The master ask initiatively, and the slaves ack passively.
* Can ensure data integrity by checksum algorithm.
* The master can resend automatically, with a feedback of resend times.
* The shared data buffer can save space and time.
* Caches sent data for resend.
* Support variable-length data part.
* Application can define their own data structure.
* Statistics for every sent and received package.

Licence
=======

Berkeley Software Distribution license
http://directory.fsf.org/wiki/License:BSD_3Clause

Contact
=======

Send me an email: 
joshuazchen@gmail.com












