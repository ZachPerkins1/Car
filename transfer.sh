ADDRESS=pi@192.168.1.244
ssh $ADDRESS "rm -rf Server; mkdir Server;"
scp -r server/* $ADDRESS:Server
ssh $ADDRESS "cd Server; make; ./app"&
cd client
python app.py
