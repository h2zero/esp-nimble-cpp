# Improvements and updates

# Server  

NimBLECharacteristic::setValue(const T &s)  
NimBLEDescriptor::setValue(const T &s)  

Now use a template to accomodate standard and custom types/values.  

**Example**
```
struct my_struct{
    uint8_t  one;
    uint16_t two;
    uint32_t four;
    uint64_t eight;
    float    fl;
}myStruct;
        
    myStruct.one = 1;
    myStruct.two = 2;
    myStruct.four = 4;
    myStruct.eight = 8;
    myStruct.fl = 1234.56;

    pCharacteristic->setValue(myStruct);
 ```
This will send the defined struct to the recieving client when read or notification sent.  

NimBLECharacteristic::getValue now takes an optional timestamp parameter which will update it's value with  
the time the last value was recieved. In addition an overloaded template has been added to retrieve the value  
as a type specified by the user.  

**Example**
```
    time_t timestamp;
    myStruct = pCharacteristic->getValue<myStruct>(&timestamp); // timestamp optional
```
<br/>

# Client  

NimBLERemoteCharacteristic::readValue(time_t\*, bool)  
NimBLERemoteDescriptor::readValue(bool)  

Have been added as templates to allow reading the values as any specified type.   

**Example**
```
struct my_struct{
    uint8_t  one;
    uint16_t two;
    uint32_t four;
    uint64_t eight;
    float    fl;
}myStruct;

    time_t timestamp;
    myStruct = pRemoteCharacteristic->readValue<myStruct>(&timestamp); // timestamp optional
```  
<br/>

NimBLERemoteCharacteristic::registerForNotify  
Has been **deprecated** as now the internally stored characteristic value is updated when notification/indication is recieved.  

NimBLERemoteCharacteristic::subscribe and NimBLERemoteCharacteristic::unsubscribe have been implemented to replace it.  
A callback is no longer requred to get the most recent value unless timing is important. Instead, the application can call NimBLERemoteCharacteristic::getValue to  
get the last updated value any time.  

In addition NimBLERemoteCharacteristic::readValue and NimBLERemoteCharacteristic::getValue take an optional timestamp parameter which will update it's value with  
the time the last value was recieved.  
<br/>

# General  
To reduce resource use all instances of std::map have been replaced with std::vector.  

Use of FreeRTOS::Semaphore has been removed as it was consuming too much ram, the related files have been left in place to accomodate application use.  

Operators `==`, `!=` and `std::string` have been added to NimBLEAddress and NimBLEUUID for easier comparison and logging.  

New constructor for NimBLEUUID(uint32_t, uint16_t, uint16_t, uint64_t) added to lower memory use vs string construction. See: [#21](https://github.com/h2zero/NimBLE-Arduino/pull/21).   

Security/pairing operations are now handled in the respective NimBLEClientCallbacks and NimBLEServerCallbacks classes, NimBLESecurity(deprecated) remains for backward compatibility.  

Many more internal improvements have been made as well, this is a brief overview. Refer to the class docs for futher information on class specifics.  
<br/>  

