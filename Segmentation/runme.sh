matlab -nodisplay -nosplash -sd /home/lab/REShAPE_Standalone/Segmentation -logfile /home/lab/REShAPE_Standalone/Segmentation/segmf.log -r "try; segmfunc('/home/lab/REShAPE_Standalone/Segmentation/segmparam.json'); catch ex; disp(strrep(ex.message,sprintf('\n'),': ')); quit(1); end; quit(0);"