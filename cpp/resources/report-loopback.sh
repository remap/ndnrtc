#!/bin/bash

cat stats-original/data-ndnrtc-loopback-producer-camera.stat | normalize-time.py - | derivative.py -m 8 - > bitrates.csv
gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30"; 
               set title ""; set tics font ",25"; 
               set xlabel "Time" font ",25"; 
               set ylabel "Bitrate (bps)" font ",25"; 
               set key outside; 
               plot "bitrates.csv" using 1:2 with lines title "Payload", 
                       "" using 1:3 with lines title "Wire"' > bitrates.png

echo -e "timestamp\tdelta\tkey" > tmp.csv && cat all.log | grep "segment-controller" | grep "received data" | awk -F'[\t:]' '{ print $1, $3}' | gawk 'match($0, /received data .*\/(k|d)\/.* ([0-9]+) bytes/, a) { if (a[1] == "d") print $1 "\t" a[2] "\t "; else print $1 "\t\t" a[2]; }' >> tmp.csv
cat tmp.csv | integral.py -i 0.1 - > segments.csv
gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30"; 
               set style fill transparent solid 0.8 noborder; 
               set title ""; 
               set tics font ",25"; 
               set xlabel "Time" font ",25"; 
               set ylabel "Received bytes" font ",25"; 
               set key outside; 
               plot "segments.csv" using 1:2 title "Delta" with boxes, 
                       "" using 1:3 title "Key" with boxes' > segments.png

cat stats-original/network-ndnrtc-loopback-producer-camera.stat | normalize-time.py - | derivative.py - > network.csv
gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Time" font ",25";
               set ylabel "Frequency (Hz)" font ",25";
               set key outside;
               plot "network.csv" using 1:2 with lines title "Interests",
                    "" using 1:3 with lines title "Data", 
                    "" using 1:4 with lines title "App Nack", 
                    "" using 1:5 with lines title "Netw Nack", 
                    "" using 1:6 with lines title "Timeouts"' > network.png

echo -e "frameNo\tdataSeg\tparitySeg\tframeType" > segments.csv && \
    cat logs-original/producer-producer-camera.log | grep "published frame" | awk -F"[\t:]" '{ print $1 $3 }' |\
    gawk 'match($0, /published frame ([0-9]+)([dk]) ([0-9]+)p .*data segments x([0-9]+) parity segments x([0-9]+)/, a) { print a[3] "\t" a[4] "\t" a[5] "\t" a[2]}' >> segments.csv
gnuplot -p -e 'set style histogram rowstacked;
               set style data histograms;
               set style fill solid; 
               set datafile separator "\t";
               set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Frame #" font ",25";
               set ylabel "# of segments" font ",25";
               set key outside;
               set xrange [0:];
               fName = "segments.csv";
               plot 
                 fName using (strcol(4) eq "d"?$2:1/0) t "Delta (Data)" lc rgb "#A4AF69",
                 fName using (strcol(4) eq "d"?$3:1/0) t "Delta (Parity)" lc rgb "#EDE580",
                 fName using (strcol(4) eq "k"?$2:1/0) t "Key (Data)" lc rgb "#D35269", 
                 fName using (strcol(4) eq "k"?$3:1/0) t "Key (Parity)" lc rgb "#F5AC72";' > segments.png
               
               # plot "segments.csv" using (strcol(4) eq "d" ? ) 2 t "Data", 
               #         "" using 3 t "Parity";' > segments.png

gnuplot -p -e 'set style histogram rowstacked;
               set style data histograms;
               set style fill solid;
               set datafile separator "\t";
               set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Time" font ",25";
               set ylabel "Milliseconds" font ",25";
               set key outside;
               set xrange [0:];
               plot "stats-original/buffer-ndnrtc-loopback-producer-camera.stat" using 2 t "Playout queue", "" using 3 t "Pending queue";' > buffer.png


gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Time" font ",25";
               set ylabel "Milliseconds" font ",25";
               set y2label "Lambda (# of frames)" font ",25";
               set key outside;
               set ytics nomirror; 
               set y2tics;
               plot "stats-original/playback-ndnrtc-loopback-producer-camera.stat" using 2 t "Lambda" axes x1y2 with lines, 
                       "" using 3 t "DRD (ms)" with lines, 
                       "" using 4 t "Latency Est. (ms)" with lines' > playback.png

#!/bin/bash

cat stats-original/data-ndnrtc-loopback-producer-camera.stat | normalize-time.py - | derivative.py -m 8 - > bitrates.csv
gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30"; 
               set title ""; set tics font ",25"; 
               set xlabel "Time" font ",25"; 
               set ylabel "Bitrate (bps)" font ",25"; 
               set key outside; 
               plot "bitrates.csv" using 1:2 with lines title "Payload", 
                       "" using 1:3 with lines title "Wire"' > bitrates.png

echo -e "timestamp\tdelta\tkey" > tmp.csv && cat all.log | grep "segment-controller" | grep "received data" | awk -F'[\t:]' '{ print $1, $3}' | gawk 'match($0, /received data .*\/(k|d)\/.* ([0-9]+) bytes/, a) { if (a[1] == "d") print $1 "\t" a[2] "\t "; else print $1 "\t\t" a[2]; }' >> tmp.csv
cat tmp.csv | integral.py -i 0.1 - > segments.csv
gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30"; 
               set style fill transparent solid 0.8 noborder; 
               set title ""; 
               set tics font ",25"; 
               set xlabel "Time" font ",25"; 
               set ylabel "Received bytes" font ",25"; 
               set key outside; 
               plot "segments.csv" using 1:2 title "Delta" with boxes, 
                       "" using 1:3 title "Key" with boxes' > segments.png

cat stats-original/network-ndnrtc-loopback-producer-camera.stat | normalize-time.py - | derivative.py - > network.csv
gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Time" font ",25";
               set ylabel "Frequency (Hz)" font ",25";
               set key outside;
               plot "network.csv" using 1:2 with lines title "Interests",
                    "" using 1:3 with lines title "Data", 
                    "" using 1:4 with lines title "App Nack", 
                    "" using 1:5 with lines title "Netw Nack", 
                    "" using 1:6 with lines title "Timeouts"' > network.png

echo -e "frameNo\tdataSeg\tparitySeg\tframeType" > segments.csv && \
    cat logs-original/producer-producer-camera.log | grep "published frame" | awk -F"[\t:]" '{ print $1 $3 }' |\
    gawk 'match($0, /published frame ([0-9]+)([dk]) ([0-9]+)p .*data segments x([0-9]+) parity segments x([0-9]+)/, a) { print a[3] "\t" a[4] "\t" a[5] "\t" a[2]}' >> segments.csv
gnuplot -p -e 'set style histogram rowstacked;
               set style data histograms;
               set style fill solid; 
               set datafile separator "\t";
               set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Frame #" font ",25";
               set ylabel "# of segments" font ",25";
               set key outside;
               set xrange [0:];
               fName = "segments.csv";
               plot 
                 fName using (strcol(4) eq "d"?$2:1/0) t "Delta (Data)" lc rgb "#A4AF69",
                 fName using (strcol(4) eq "d"?$3:1/0) t "Delta (Parity)" lc rgb "#EDE580",
                 fName using (strcol(4) eq "k"?$2:1/0) t "Key (Data)" lc rgb "#D35269", 
                 fName using (strcol(4) eq "k"?$3:1/0) t "Key (Parity)" lc rgb "#F5AC72";' > segments.png
               
               # plot "segments.csv" using (strcol(4) eq "d" ? ) 2 t "Data", 
               #         "" using 3 t "Parity";' > segments.png

gnuplot -p -e 'set style histogram rowstacked;
               set style data histograms;
               set style fill solid;
               set datafile separator "\t";
               set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Time" font ",25";
               set ylabel "Milliseconds" font ",25";
               set key outside;
               set xrange [0:];
               plot "stats-original/buffer-ndnrtc-loopback-producer-camera.stat" using 2 t "Playout queue", "" using 3 t "Pending queue";' > buffer.png


gnuplot -p -e 'set terminal png size 2500,1500 enhanced font ",30";
               set title "";
               set tics font ",25";
               set xlabel "Time" font ",25";
               set ylabel "Milliseconds" font ",25";
               set y2label "Lambda (# of frames)" font ",25";
               set key outside;
               set ytics nomirror; 
               set y2tics;
               plot "stats-original/playback-ndnrtc-loopback-producer-camera.stat" using 2 t "Lambda" axes x1y2 with lines, 
                       "" using 3 t "DRD (ms)" with lines, 
                       "" using 4 t "Latency Est. (ms)" with lines' > playback.png

function frac(){
    p1=${1%.*}
    p2=${2%.*}
    f=$(($p1*100/$p2))
    echo $f
}

framesCaptured=`tail -1 stats-original/frames-publish-ndnrtc-loopback-producer-camera.stat | awk '{ print $2 }'`
framesPublished=`tail -1 stats-original/frames-publish-ndnrtc-loopback-producer-camera.stat | awk '{ print $4 }'`
framesDropped=`tail -1 stats-original/frames-publish-ndnrtc-loopback-producer-camera.stat | awk '{ print $5 }'`

echo ""
echo "Producer Stats:"
echo "  frames captured:  ${framesCaptured}"
echo "  frames published: ${framesPublished}"
echo "  frames dropped:   ${framesDropped}"

framesRequested=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $2 }'`
framesAssembled=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $3 }'`
framesRecovered=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $5 }'`
framesRescued=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $6 }'`
framesPlayed=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $4 }'`
framesIncomplete=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $7 }'`
framesSkipped=`tail -1 stats-original/frames-ndnrtc-loopback-producer-camera.stat | awk '{ print $8 }'`

interestSent=`tail -1 stats-original/network-ndnrtc-loopback-producer-camera.stat | awk '{ print $2 }'`
segRcvd=`tail -1 stats-original/network-ndnrtc-loopback-producer-camera.stat | awk '{ print $3 }'`
appNacks=`tail -1 stats-original/network-ndnrtc-loopback-producer-camera.stat | awk '{ print $4 }'`
nacks=`tail -1 stats-original/network-ndnrtc-loopback-producer-camera.stat | awk '{ print $5 }'`
timeouts=`tail -1 stats-original/network-ndnrtc-loopback-producer-camera.stat | awk '{ print $6 }'`
rtx=`tail -1 stats-original/network-ndnrtc-loopback-producer-camera.stat | awk '{ print $7 }'`

echo ""
echo "Consumer Stats ($consumer):"
echo "  frames requested:   ${framesRequested}"
echo "  frames assembled:   ${framesAssembled} (`frac $framesAssembled $framesRequested`% of requested)"
echo "  frames incomplete:  ${framesIncomplete} (`frac $framesIncomplete $framesRequested`% of requested)"
echo "  frames played:      ${framesPlayed} (`frac $framesPlayed $framesAssembled`% of assembled)"
echo "       recovered:   ${framesRecovered} (`frac $framesRecovered $framesPlayed`% of played)"
echo "       rescued:     ${framesRescued} (`frac $framesRescued $framesPlayed`% of played)"
echo "  frames skipped:     ${framesSkipped} (`frac $framesSkipped $framesAssembled`% of assembled)"
echo ""
echo "  interests sent:     ${interestSent}"
echo "  retransmissions:    ${rtx} (`frac $rtx $interestSent`%)"
echo "  segments recevied:  ${segRcvd} (`frac $segRcvd $interestSent`%)"
echo "  timeouts:           ${timeouts} (`frac $timeouts $interestSent`%)"
echo "  app nacks:          ${appNacks} (`frac $appNacks $interestSent`%)"
echo "  network nacks:      ${nacks} (`frac $nacks $interestSent`)"

p1=${framesPlayed%.*}
p2=${framesRequested%.*}
score=$(($p1*100/$p2))
echo " --- "
echo "  fetching efficiency: ${score}%"