# Technical Documentation

## Background
The engine is built out from the vulkan tutorial from Vulkan.org. Some changes has been made to this due to updates in the language as a whole on V20+ making the old code invalid.

## Memory management
This program used raii for all the major objects meaning it is automaticly cleaned up.
