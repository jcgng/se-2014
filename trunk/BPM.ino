void saveBPM() {
    if(lastBPM[9]==0) { // not filled
      for(int i=0;i<10;i++) {
        if(lastBPM[i]==0) {
          lastBPM[i] = BPM; break;
        }
      }
    } else { // filled
      for(int i=0;i<=8;i++) {
        lastBPM[i] = lastBPM[i+1];
      }
      lastBPM[9] = BPM;
    }
}

