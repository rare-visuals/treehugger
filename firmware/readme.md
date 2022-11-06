


## REST API

### GET '/api'

### POST '/api/wifi'

### POST '/api/device'

### POST '/api/light

### GET '/api/modules/led/

 ```
 {
    "channels": {
        "1": {
            "active":true,
            "segments": [15,30]
        },
        "2": {
            "active":false
        }
    }
 }

 ```

### POST '/api/modules/led/

 ```
{
    "channels":{
        "1":{
            "active": true,
            "segments":{
                "0": {
                    "leds":15,
                    "mode":"full"
                },
                "1": {
                    "leds":30,
                    "mode":"half"
                }
            }
        },
        "2":{
            "active": true,
            "segments": {
                "0": {
                    "leds":30,
                    "mode":"solid"
                }
            }
        }
    }
}
 ```


### POST '/api/modules/led/setup'

 ```
{
    "channels":{
        "1":{
            "segments":{
                "0": "inactive",
                "1":"inactive"
            }
        },
        "2":{
            "segments": {
                "0": "selected",
                "1":"focus"
            }
        }
    }
}
 ```

