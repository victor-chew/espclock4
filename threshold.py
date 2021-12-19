import sys

fwd_speedup = 8
rev_speedup = 4

def main():
	for hh in range(3, 10):
		for mm in range(0, 60):
			for ss in range(0, 60):
				fwd_duration = hh*60*60 + mm*60 + ss
				t1 = calc_sync_time(+1, fwd_duration, fwd_speedup)
				t2 = calc_sync_time(-1, (12*60*60) - fwd_duration, rev_speedup)
				if t1 == t2: 
					print('Threshold =', hh, ':', mm, ':', ss)
					sys.exit()

def calc_sync_time(direction, duration, speedup):
	result = 0;
	while(duration > speedup*2):
		half = int(duration/2)
		interval = int(half / speedup)
		result += interval
		duration = duration - (interval * speedup) + (interval * direction)
	result += int(duration/speedup)
	return result

if __name__ == "__main__": main()
