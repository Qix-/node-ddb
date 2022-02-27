# DDB

A native libav&ast; binding that extracts fixed-sized
frames from video and images, for use with image hashing
and the like.

> **NOTE:** This library is HIGHLY specialized and *probably*
> isn't what you're looking for. It comes with a few hefty
> warning labels and assumptions about how you'll use it
> in order to stay performant.

I will re-iterate: **this is most likely _not_ the module you're
looking for.**

## **WARNING!**

Absolute **under no circumstances** should you save the `Buffer` reference
passed to the `read` and `frames` callbacks. Do not store it, read from it,
or write to it outside of the lifetime of the `read` call!

**YOU'VE BEEN WARNED. THIS PACKAGE COMES WITH NO WARRANTY.**

# License

GPL3, because everything else is GPL. Don't really have a choice. Sorry.
Perhaps stop licensing things with viral licenses.
