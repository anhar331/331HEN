#!/usr/bin/env python3
"""Refresh the public CDN source and compare with the canonical GitHub JSON."""
import json,time,urllib.request
from pathlib import Path
MIRROR='https://cdn.jsdelivr.net/gh/anhar331/331HEN@main/payloads.json'
PURGE='https://purge.jsdelivr.net/gh/anhar331/331HEN@main/payloads.json'
ROOT=Path(__file__).resolve().parents[1]
def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'331HEN-source-mirror-verifier'}),timeout=30) as response:
        if response.status!=200: raise ValueError('HTTP failure')
        return json.loads(response.read(1048577))
def main():
    expected=json.loads((ROOT/'payloads.json').read_text())
    try:
        print('CDN refresh:',fetch(PURGE).get('status','requested'))
    except Exception as error:
        print('CDN purge unavailable; checking existing mirror:',type(error).__name__)
    for attempt in range(8):
        try:
            public=fetch(MIRROR)
            if public==expected:
                print('Verified identical source:',MIRROR)
                return
        except Exception as error:
            print('Mirror attempt failed:',type(error).__name__)
        if attempt<7: time.sleep(10)
    raise RuntimeError('Mirror did not match the approved repository JSON; primary source is unchanged')
if __name__=='__main__':main()
