import socket, cv2, time, threading, json, Queue
import Tkinter as tk
from Tkinter import E, W, N, S, NW, SW, LEFT
import numpy as np

from PIL import ImageTk, Image
from select import select
from multiprocessing.pool import ThreadPool

IP = "192.168.1.244" # 244

VISION_ADDRESS = (IP, 3000) # The address of the car
CONTROLS_ADDRESS = (IP, 3001)
MAIN_ADDRESS = (IP, 3002)

IMAGE_RES = (640, 480) # The resolution of the image that we plan to receive
TIMEOUT = 10 # How long we're willing to wait before killing the program

KEY_W = 87
KEY_A = 65
KEY_S = 83
KEY_D = 68

# ------------------- GUI Definition ------------------- #

# Basic Tk frame definition
class App(tk.Frame):
	def __init__(self, master=None):
		tk.Frame.__init__(self, master)
		self.grid()
		self.make_gui()
		self.master.title = "Car Program"
		self.keys = {}

		self.master.bind("<KeyPress>", self._key_press)
		self.master.bind("<KeyRelease>", self._key_release)

	def make_gui(self):
		self.img = None
		self.image = tk.Label(self, image=self.img, bd=0)
		self.image.grid(row=0, column=0)

		self.control_panel = tk.Frame(self, background="#3b3b3b", width=640)
		self.control_panel.grid(row=1, column=0, sticky=E+W+N+S)

		self.text = tk.StringVar()
		self.text.set("Test motherfucker")

		self.img_norm = ImageTk.PhotoImage(Image.open("assets/normal.png"))
		self.img_err = ImageTk.PhotoImage(Image.open("assets/error.png"))
		self.img_warn = ImageTk.PhotoImage(Image.open("assets/warn.png"))

		face = "Open Sans Regular"

		inner_frame = tk.Frame(self.control_panel, bg="#3b3b3b")
		inner_frame.grid(row=0, column=0, pady=10)
		# Note to self: Making guis is awful gruntwork
		labels = [["remote", ["vision", "controls"]], ["local",["vision", "controls", "comms"]]]
		labels_visual = [["Remote Systems", ["Vision", "Controls"]], ["Local systems",["Vision", "Controls", "Communications"]]]
		self.statuses = {}

		for i,label in enumerate(labels):
			self.statuses[label[0]] = {}
			frame = tk.Frame(inner_frame, bg="#2d2d2d")
			frame.grid(row=i, column=0, ipadx=7, pady=10, sticky=W, ipady=4)
			frame.grid_columnconfigure(1, minsize=209)
			frame.grid_columnconfigure(2, minsize=209)
			l = tk.Label(frame, bg="#2d2d2d", text=labels_visual[i][0], justify=LEFT, font=(face, 15), fg="#dedede")
			l.grid(column=1,row=0, columnspan=2, sticky=W)
			tk.Frame(frame, width=10, bg="#2d2d2d").grid(column=0, row=0, rowspan=2) # spacer
			for j,subsystem in enumerate(label[1]):
				self.statuses[label[0]][subsystem] = {}
				subframe = tk.Frame(frame, bg="#414141")
				subframe.grid(column=1+j%2, row=1+j/2, sticky=W+N+E+S, padx=3, pady=3, ipadx=4)
				self.statuses[label[0]][subsystem]["ind"] = tk.Label(subframe, bg="#414141", image=self.img_norm, bd=0)
				self.statuses[label[0]][subsystem]["ind"].grid(row=0, column=0, sticky=W, padx=3, pady=(3,0))
				tk.Label(subframe, bg="#414141", text=labels_visual[i][1][j], justify=LEFT, font=(face, 12), fg="#b6b6b6", pady=0, bd=0).grid(row=0, column=1, sticky=SW, pady=0)
				self.statuses[label[0]][subsystem]["msg"] = tk.Label(subframe, bg="#414141", text="No status yet", justify=LEFT, font=(face, 10), fg="#a1a1a1", pady=0, bd=0)
				self.statuses[label[0]][subsystem]["msg"].grid(row=1, column=1, sticky=NW, pady=(0, 4))


	def _key_press(self, event):
		self.keys[event.keycode] = True

	def _key_release(self, event):
		self.keys[event.keycode] = False

	def is_key_down(self, keycode):
		if keycode in self.keys:
			return self.keys[keycode]
		else:
			return False

	def set_status(self, loc, label, msg, ind):
		img = self.img_norm

		if ind == "warn":
			img = self.img_warn
		elif ind == "err":
			img = self.img_err

		subsystem = self.statuses[loc][label]
		subsystem["msg"].configure(text=msg)
		subsystem["ind"].configure(image=img)



# ------------------- All processes ------------------- #

def connect_continuous(c_sock, address, time_between):
	count = 1

	while True:
		print "Attempting connection to host (try #" + str(count) + ")"

		try:
			c_sock.connect(address)
			print "Connection successful"
			return
		except:
			pass

		time.sleep(time_between)
		count += 1

def wait_on_read_socket(sock, timeout):
	result = select([sock], [], [], timeout)
	if len(result[0]) == 0:
		raise socket.error("Host took too long to respond")

def recv_check(sock, bytes):
	msg = sock.recv(bytes)

	if len(msg) == 0:
		raise socket.error("Host response was empty string")
	return msg

def keep_trying(func, args=()):
	while True:
		func(*args)

def set_default_status(key):
	with app_lock:
		app.set_status("local", key, "Operating Normally", "norm")

def set_connect_status(key):
	with app_lock:
		app.set_status("local", key, "Connecting...", "warn")

def set_lost_status(key):
	with app_lock:
		app.set_status("local", key, "Connection Lost", "err")

# ------------------- Vision ------------------- #

# Initiate the main loop which grabs a frame and then displays the frame
def vision_main(): 
	set_connect_status("vision")

	c_sock = socket.socket() # TCP socket for comms
	d_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP Socket for video data

	print "Starting vision..."
	connect_continuous(c_sock, VISION_ADDRESS, 5)
	print "Vision online"

	d_sock.sendto("START", VISION_ADDRESS)

	c_sock.setblocking(0)
	d_sock.setblocking(0)

	image = ""
	frame_call = None

	set_default_status("vision")

	while True:
		frame_call = pool.apply_async(get_frame, (c_sock, d_sock))
		display_frame(image)

		try: 
			image = frame_call.get()
		except:
			set_lost_status("vision")
			
			time.sleep(5)
			break

# Get the frame and process it into a raw image
def get_frame(c_sock, d_sock):
	chunks = []
	c_sock.send("OKAY") # We're ready to begin receiving the frames
	count = 0

	wait_on_read_socket(c_sock, TIMEOUT)
	count = int(recv_check(c_sock, 1024))

	print count

	# We receive the frames in a collection of chunks from our host machine
	while len(chunks) < count:
		try:
			wait_on_read_socket(d_sock, 1)
			data, addr = d_sock.recvfrom(4096)
			chunks.append(data)
		except:
			print "Missing image chunk, assuming data loss and continuing"
			break

	return "".join(chunks)


def display_frame(frame):
	raw = cv2.imdecode(np.fromstring(frame, dtype=np.uint8), 1)

	if raw is not None:
		decoded = cv2.cvtColor(raw, cv2.COLOR_BGR2RGB)
		final = ImageTk.PhotoImage(Image.frombytes("RGB", IMAGE_RES, decoded))

		with app_lock:
			app.img = final
			app.image.configure(image=app.img) # Tell Tkinter that we actually changed the image


# ------------------- Controls ------------------- #

def controls_main():
	set_connect_status("controls")
	print "Starting controls..."
	c_sock = socket.socket()
	connect_continuous(c_sock, CONTROLS_ADDRESS, 5)
	c_sock.setblocking(0)
	print "Controls online"
	set_default_status("controls")

	while True:
		try:
			wait_on_read_socket(c_sock, 10)
			c = recv_check(c_sock, 1024)
			send_control_data(c_sock)
		except socket.error:
			print "Controls ded"
			set_lost_status("controls")
			time.sleep(5)
			break


def send_control_data(c_sock):
	data = {}
	y = 0
	x = 0

	with app_lock:
		if app.is_key_down(KEY_W):
			y = 255
		elif app.is_key_down(KEY_S):
			y = -255

		if app.is_key_down(KEY_D):
			x = 255
		elif app.is_key_down(KEY_A):
			x = -255

	data["x"] = x
	data["y"] = y

	sendstr = json.dumps(data)
	sendstr += "\x00" # NUL terminator for the C stuffs
	c_sock.send(sendstr)


# ------------------- Comms ------------------- #

def set_errors():
	with app_lock:
		app.set_status("remote", "vision", "Comms Down", "err")
		app.set_status("remote", "controls", "Comms Down", "err")

def comms_main(queue):
	set_errors()
	set_connect_status("comms")

	c_sock = socket.socket()
	print "Attempting connection to the vehicle..."
	connect_continuous(c_sock, MAIN_ADDRESS, 5)
	c_sock.setblocking(0)

	set_default_status("comms")

	while True:
		try:
			try:
				command = queue.get(timeout=0.1)
				c_sock.send(command)
			except Queue.Empty:
				c_sock.send("POLL")

			wait_on_read_socket(c_sock, TIMEOUT)

			data = json.loads(recv_check(c_sock, 1024))

		except socket.error:
			set_errors()
			set_lost_status("comms")
			print "socket erroe"
			time.sleep(5)
			break

		for key in data:
			with app_lock:
				app.set_status("remote", key, data[key]["msg"], data[key]["ind"])


# ------------------- Main ------------------- #

def main():
	global pool, app, app_lock

	pool = ThreadPool(processes = 3)
	app = App()
	app_lock = threading.Lock()

	command_queue = Queue.Queue()

	# threading.Thread(target=keep_trying, args=(comms_main, (command_queue,))).start()
	threading.Thread(target=keep_trying, args=(vision_main,)).start()
	# threading.Thread(target=keep_trying, args=(controls_main,)).start()

	while True:
		with app_lock:
			app.update()

		time.sleep(1.0/60)


if __name__ == "__main__":
	main()
