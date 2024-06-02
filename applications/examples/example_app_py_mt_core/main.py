# ASEQサンプルプログラム内のLinux examples\applications\examples\windows\example_app_c_mt_core\main.c のPython版。一部正常に動かず

import time
from ctypes import POINTER, byref, c_uint16, c_uint32, c_uint8
from threading import Thread, Lock, Event
from random import randint
from aseq import Spectrometer

class SpectrometerThread(Thread):
    def __init__(self, index, counter, timer_start, timer_stop, stdout_lock, measure_event, measure_lock):
        super().__init__()
        self.index = index
        self.counter = counter
        self.timer_start = timer_start
        self.timer_stop = timer_stop
        self.stdout_lock = stdout_lock
        self.measure_event = measure_event
        self.measure_lock = measure_lock

    def run(self):
        with Spectrometer() as spec:
            self.initialize_spectrometer(spec)
            # self.read_flash_example(spec) # 失敗するため保留
            self.read_frames_example(spec)

    def initialize_spectrometer(self, spec):
        spec.connect()
        spec.num_of_scans = 1
        spec.num_of_blank_scans = 0
        spec.scan_mode = 3
        spec.exposure_time = 10000 * (self.index + 1)

    def read_flash_example(self, spec):
        flash_offset = 0
        flash_bytes_to_read = 100
        flash_bytes_to_write = flash_bytes_to_read

        buffer_size = max(flash_bytes_to_read, flash_bytes_to_write)
        buffer = bytearray(buffer_size)

        result = spec.flash.erase() # 失敗する。原因不明
        if result != 0:
            self.mutexed_print(f"readFlashExample on device {self.index}: eraseFlash() failed with code {result}")
            return

        buffer = bytearray(spec.flash.read(flash_bytes_to_read, flash_offset)) # これは成功する
        if len(buffer) != flash_bytes_to_read:
            self.mutexed_print(f"readFlashExample on device {self.index}: readFlash() failed with incomplete read")
            return

        self.print_flash_buffer(buffer)

        self.randomize_buffer(buffer)
        result = spec.flash.write(bytes(buffer), flash_offset)  # eraseを行っていない場合、警告 (only empty memory locations can be written to)
        if result != 0:
            self.mutexed_print(f"readFlashExample on device {self.index}: writeFlash() failed with code {result}")
            return

        buffer = bytearray(spec.flash.read(flash_bytes_to_read, flash_offset))
        if len(buffer) != flash_bytes_to_read:
            self.mutexed_print(f"readFlashExample on device {self.index}: readFlash() failed after a writing operation")
            return

        self.print_flash_buffer(buffer)

    def print_flash_buffer(self, buffer):
        with self.stdout_lock:
            print(f"readFlashExample from thread {self.index} (offset: 0, bytesToRead: {len(buffer)}): ", end="")
            for byte in buffer:
                print(byte, end=" ")
            print()

    def randomize_buffer(self, buffer):
        for i in range(len(buffer)):
            buffer[i] = randint(0, 255)

    def read_frames_example(self, spec):  # setAcquisitionParametersの処理はinitialize_spectrometerで行っている
        spec.memory.clear()
        spec.trigger()
        frame_count = 0
        frames_required = 11

        # frames_required回frameを取得しそれぞれの経過時間を出力する。最終回のみframeの値(0~65535)をすべて出力する
        while frame_count < frames_required:
            status = spec.status()
            if status & Spectrometer.Status.MEMORY_FULL:
                spec.memory.clear()
                spec.trigger()
                continue

            if len(spec.memory) > 0:
                if frame_count == frames_required - 1:
                    with self.measure_lock:
                        self.counter -= 1
                        if self.counter == 0:
                            self.measure_event.set()

                    self.measure_event.wait()
                    self.timer_start[self.index] = time.time()

                local_start = time.time()
                frame = spec.memory[0]
                local_stop = time.time()

                if frame_count == frames_required - 1:
                    self.timer_stop[self.index] = time.time()

                print(f"local getFrame execution time on device {self.index} in seconds: {local_stop - local_start}")

                if frame_count == frames_required - 1:
                    self.print_frame_buffer(frame)

                frame_count += 1

    def print_frame_buffer(self, frame_buffer):
        with self.stdout_lock:
            print(f"exampleReadFrames from thread {self.index} (numOfPixelsInFrame: {len(frame_buffer)}): ", end="")
            for pixel in frame_buffer:
                print(pixel, end=" ")
            print("\n")

    def mutexed_print(self, message):
        with self.stdout_lock:
            print(message)

def example_get_and_clear_devices_info():
    devices = Spectrometer.device_list()
    for index, serial in enumerate(devices):
        print(f"HID device serial: {serial} (index: {index})")

def main():
    device_count = Spectrometer.device_count()
    print(f"Number of devices: {device_count}")

    example_get_and_clear_devices_info()

    threads = []
    stdout_lock = Lock()
    measure_event = Event()
    measure_lock = Lock()

    timer_starts = [0] * device_count
    timer_stops = [0] * device_count
    counter = device_count

    for index in range(device_count):
        thread = SpectrometerThread(index, counter, timer_starts, timer_stops, stdout_lock, measure_event, measure_lock)
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

    min_start = min(timer_starts)
    max_stop = max(timer_stops)
    total_time = max_stop - min_start
    print(f"simultaneous getFrame execution time on all devices in seconds: {total_time}")

if __name__ == "__main__":
    main()
