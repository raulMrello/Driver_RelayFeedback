# Driver_RelayFeedback

Driver_RelayFeedback is an ```arm mbed api``` compatible relay feedback driver. It can be controlled through a ```InterruptIn``` and allows a precise control of an associated Relay driver. This driver can be used in conjuction with Driver_Zerocross to auto-calibrate relay switching operations, to ensure that the relay physically switches at zero-cross level.

Also provides a physical verification of the relay's state.



---
---
  
## Changelog

---
### **17 Jan 2019**
- [x] Added ```component.mk```
- [x] Initial commit
