import os
import struct

ESP_SIZE = 64 * 1024 * 1024
SECTOR_SIZE = 512
CLUSTER_SIZE = 4096

NUM_SECTORS = ESP_SIZE // SECTOR_SIZE
NUM_CLUSTERS = (NUM_SECTORS - 32) // (CLUSTER_SIZE // SECTOR_SIZE)

def main():
    with open('esp.img', 'wb') as f: