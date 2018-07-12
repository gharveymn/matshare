function ret = generatevariable(classid, makesparse, dims)
	switch(classid)
		case(3)
			% 	3	mxLOGICAL_CLASS,
			if(makesparse)
				ret = sprand(dims(1),dims(2),rand) > 0.5;
			else
				ret = rand(dims) > 0.5;
			end
		case(4)
			% 	4	mxCHAR_CLASS,
			ret = char(randi([0,65535],dims));
		case(5)
			% 	5	mxVOID_CLASS,
			%reserved, make a double instead
			ret = rand(dims,'double');
		case(6)
			% 	6	mxDOUBLE_CLASS,
			if(makesparse)
				ret = sprand(dims(1),dims(2),rand);
			else
				ret = rand(dims,'double');
			end
		case(7)
			% 	7	mxSINGLE_CLASS,
			ret = rand(dims,'single');
		case(8)
			% 	8	mxINT8_CLASS,
			ret = randi(intmax('int8'),dims,'int8');
		case(9)
			% 	9	mxUINT8_CLASS,
			ret = randi(intmax('uint8'),dims,'uint8');
		case(10)
			% 	10	mxINT16_CLASS,
			ret = randi(intmax('int16'),dims,'int16');
		case(11)
			% 	11	mxUINT16_CLASS,
			ret = randi(intmax('uint16'),dims,'uint16');
		case(12)
			% 	12	mxINT32_CLASS,
			ret = randi(intmax('int32'),dims,'int32');
		case(13)
			% 	13	mxUINT32_CLASS,
			ret = randi(intmax('uint32'),dims,'uint32');
		case(14)
			% 	14	mxINT64_CLASS,
			f = @() uint64( randi(intmax('uint32'), dims, 'uint32') );
			% bitshift and bitor to convert into a proper uint64        
			ret = int64(bitor( bitshift(f(),32), f() ));
		case(15)
			% 	15	mxUINT64_CLASS,
			f = @() uint64( randi(intmax('uint32'), dims, 'uint32') );
			% bitshift and bitor to convert into a proper uint64        
			ret = bitor( bitshift(f(),32), f() );
	end
end