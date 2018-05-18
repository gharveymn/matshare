function sig = emumemhead(seg_sz)
	seg_sz = uint64(seg_sz);
	sz = 8;
	multiplier = uint64(16);
	mm1 = uint64(15);
	
	sig = zeros(1,sz);
	
	if(seg_sz > 0)
		
		sig(1) = bitand(idivide(seg_sz + mm1, multiplier), multiplier - 1) * multiplier;
		
		for i = 2:4
			multiplier = multiplier ^ 2;
			sig(i) = bitand(idivide(seg_sz + mm1, multiplier), multiplier - 1);
		end
	end
	
	sig = uint8(sig);
	
end