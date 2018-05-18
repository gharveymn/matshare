function sig = emumemhead32(seg_sz)
	seg_sz = uint32(seg_sz);
	sz = 4;
	multiplier = uint32(8);
	mm1 = uint32(7);
	
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