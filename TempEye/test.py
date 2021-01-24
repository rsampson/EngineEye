#!/usr/bin/env python

# WS server that sends messages at random intervals

import asyncio
import datetime
import random
import websockets

async def time(websocket, path):
    while True:
        now = random.randint(90,270)
        json = "{\"temp2\":";
        json += str(now);
        json += "}";
        
        await websocket.send(json)
        await asyncio.sleep(random.random() * 3)

start_server = websockets.serve(time, "127.0.0.1", 81)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
