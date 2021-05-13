#include "util.h"

/*
 * To be used to start a timing measurement.
 * It returns the current time.
 *
 * Inspired by:
 *  https://github.com/google/highwayhash/blob/master/highwayhash/tsc_timer.h
 */
uint64_t start_time(void)
{
	uint64_t t;
	asm volatile(
		"lfence\n\t"
		"rdtsc\n\t"
		"shl $32, %%rdx\n\t"
		"or %%rdx, %0\n\t"
		// "lfence"
		: "=a"(t) /*output*/
		:
		: "rdx", "memory", "cc");
	return t;
}

/*
 * To be used to end a timing measurement.
 * It returns the current time.
 *
 * Inspired by:
 *  https://github.com/google/highwayhash/blob/master/highwayhash/tsc_timer.h
 */
uint64_t stop_time(void)
{
	uint64_t t;
	asm volatile(
		"rdtscp\n\t"
		"shl $32, %%rdx\n\t"
		"or %%rdx, %0\n\t"
		"lfence"
		: "=a"(t) /*output*/
		:
		: "rcx", "rdx", "memory", "cc");
	return t;
}

/*
 * Appends the given string to the linked list which is pointed to by the given head
 */
void append_string_to_linked_list(struct Node **head, void *addr)
{
	struct Node *current = *head;

	// Create the new node to append to the linked list
	struct Node *new_node = malloc(sizeof(*new_node));
	new_node->address = addr;
	new_node->next = NULL;

	// If the linked list is empty, just make the head to be this new node
	if (current == NULL)
		*head = new_node;

	// Otherwise, go till the last node and append the new node after it
	else {
		while (current->next != NULL)
			current = current->next;

		current->next = new_node;
	}
}

/*
 * The argument addr should be the physical address, but in some cases it can be
 * the virtual address and this will still work. Here is why.
 *
 * Normal pages are 4KB (2^12) in size, meaning that the rightmost 12 bits of
 * the virtual address have to be the same in the physical address since they
 * are needed as offset for page table in the translation.
 * 
 * Huge pages have a size of 2MB (2^21), meaning that the rightmost 21 bits of
 * the virtual address have to be the same in the physical address since they
 * are needed as offset for page table in the translation. 
 * 
 * Since to find the set in the L1 we only need bits [11-6], then the virtual
 * address, either with normal or huge pages, is enough to get the set index.
 * 
 * Since to find the set in the L2 and LLC we only need bits [15-6], then the
 * virtual address, with huge pages, is enough to get the set index.
 * 
 * To visually understand why, see the presentations here:
 *  https://cs.adelaide.edu.au/~yval/Mastik/
 */
uint64_t get_cache_set_index(uint64_t addr, int cache_level)
{
	uint64_t index;

	if (cache_level == 1) {
		index = (addr)&L1_SET_INDEX_MASK;

	} else if (cache_level == 2) {
		index = (addr)&L2_SET_INDEX_MASK;

	} else if (cache_level == 3) {
		index = (addr)&LLC_SET_INDEX_PER_SLICE_MASK;

	} else {
		exit(EXIT_FAILURE);
	}

	return index >> CACHE_BLOCK_SIZE_LOG;
}

uint64_t find_next_address_on_slice(void *va, uint8_t desired_slice)
{
	uint64_t offset = 0;

	// Slice mapping will change for each cacheline which is 64 Bytes
	while (desired_slice != get_cache_slice_index(va + offset)) {
		offset += CACHE_BLOCK_SIZE;
	}
	return offset;
}

uint64_t find_next_address_on_slice_and_set(void *va, uint8_t desired_slice, uint32_t desired_set)
{
	uint64_t offset = 0;

	// Slice mapping will change for each cacheline which is 64 Bytes
	// NOTE: We are also ensuring that the addresses are on cache set 2
	// This is because otherwise the next time we run this program we might
	// get addresses on different sets and that might impact the latency regardless
	// of ring-bus contention.
	while (get_cache_set_index((uint64_t)va + offset, 3) != desired_set ||
		   desired_slice != get_cache_slice_index((void *)((uint64_t)va + offset))) {
		offset += CACHE_BLOCK_SIZE;
	}
	return offset;
}

/*
 * Stores the binary representation of hex_string into bin_string.
 * Make sure to pass a valid bin_string with enough space!
 */
void hexToBin(char *hex_string, char *bin_string)
{
	int i, j;

	for (j = i = 0; hex_string[i]; ++i, j += 4) {
		switch (hex_string[i]) {
		case '0':
			strcpy(bin_string + j, "0000");
			break;
		case '1':
			strcpy(bin_string + j, "0001");
			break;
		case '2':
			strcpy(bin_string + j, "0010");
			break;
		case '3':
			strcpy(bin_string + j, "0011");
			break;
		case '4':
			strcpy(bin_string + j, "0100");
			break;
		case '5':
			strcpy(bin_string + j, "0101");
			break;
		case '6':
			strcpy(bin_string + j, "0110");
			break;
		case '7':
			strcpy(bin_string + j, "0111");
			break;
		case '8':
			strcpy(bin_string + j, "1000");
			break;
		case '9':
			strcpy(bin_string + j, "1001");
			break;
		case 'A':
			strcpy(bin_string + j, "1010");
			break;
		case 'B':
			strcpy(bin_string + j, "1011");
			break;
		case 'C':
			strcpy(bin_string + j, "1100");
			break;
		case 'D':
			strcpy(bin_string + j, "1101");
			break;
		case 'E':
			strcpy(bin_string + j, "1110");
			break;
		case 'F':
			strcpy(bin_string + j, "1111");
			break;
		default:
			printf("invalid character %c\n", hex_string[i]);
			strcpy(bin_string + j, "0000");
			break;
		}
	}
}

/*
 * Function that sets memory to zero and is not optimized out by the compiler.
 */
void zeroize(void *pointer, size_t size_data)
{
	volatile uint8_t *p = pointer;
	while (size_data--)
		*p++ = 0;
}

/*
 * Generates a random uint64_t value
 */
uint64_t rand_uint64_slow(void)
{
	uint64_t r = 0;
	for (int i = 0; i < 64; i++) {
		r = r * 2 + rand() % 2;
	}
	return r;
}

void flush_l1i(void)
{
	asm volatile(
		R"(
		.align 64
		label1: jmp label2
		.align 64
		label2: jmp label3
		.align 64
		label3: jmp label4
		.align 64
		label4: jmp label5
		.align 64
		label5: jmp label6
		.align 64
		label6: jmp label7
		.align 64
		label7: jmp label8
		.align 64
		label8: jmp label9
		.align 64
		label9: jmp label10
		.align 64
		label10: jmp label11
		.align 64
		label11: jmp label12
		.align 64
		label12: jmp label13
		.align 64
		label13: jmp label14
		.align 64
		label14: jmp label15
		.align 64
		label15: jmp label16
		.align 64
		label16: jmp label17
		.align 64
		label17: jmp label18
		.align 64
		label18: jmp label19
		.align 64
		label19: jmp label20
		.align 64
		label20: jmp label21
		.align 64
		label21: jmp label22
		.align 64
		label22: jmp label23
		.align 64
		label23: jmp label24
		.align 64
		label24: jmp label25
		.align 64
		label25: jmp label26
		.align 64
		label26: jmp label27
		.align 64
		label27: jmp label28
		.align 64
		label28: jmp label29
		.align 64
		label29: jmp label30
		.align 64
		label30: jmp label31
		.align 64
		label31: jmp label32
		.align 64
		label32: jmp label33
		.align 64
		label33: jmp label34
		.align 64
		label34: jmp label35
		.align 64
		label35: jmp label36
		.align 64
		label36: jmp label37
		.align 64
		label37: jmp label38
		.align 64
		label38: jmp label39
		.align 64
		label39: jmp label40
		.align 64
		label40: jmp label41
		.align 64
		label41: jmp label42
		.align 64
		label42: jmp label43
		.align 64
		label43: jmp label44
		.align 64
		label44: jmp label45
		.align 64
		label45: jmp label46
		.align 64
		label46: jmp label47
		.align 64
		label47: jmp label48
		.align 64
		label48: jmp label49
		.align 64
		label49: jmp label50
		.align 64
		label50: jmp label51
		.align 64
		label51: jmp label52
		.align 64
		label52: jmp label53
		.align 64
		label53: jmp label54
		.align 64
		label54: jmp label55
		.align 64
		label55: jmp label56
		.align 64
		label56: jmp label57
		.align 64
		label57: jmp label58
		.align 64
		label58: jmp label59
		.align 64
		label59: jmp label60
		.align 64
		label60: jmp label61
		.align 64
		label61: jmp label62
		.align 64
		label62: jmp label63
		.align 64
		label63: jmp label64
		.align 64
		label64: jmp label65
		.align 64
		label65: jmp label66
		.align 64
		label66: jmp label67
		.align 64
		label67: jmp label68
		.align 64
		label68: jmp label69
		.align 64
		label69: jmp label70
		.align 64
		label70: jmp label71
		.align 64
		label71: jmp label72
		.align 64
		label72: jmp label73
		.align 64
		label73: jmp label74
		.align 64
		label74: jmp label75
		.align 64
		label75: jmp label76
		.align 64
		label76: jmp label77
		.align 64
		label77: jmp label78
		.align 64
		label78: jmp label79
		.align 64
		label79: jmp label80
		.align 64
		label80: jmp label81
		.align 64
		label81: jmp label82
		.align 64
		label82: jmp label83
		.align 64
		label83: jmp label84
		.align 64
		label84: jmp label85
		.align 64
		label85: jmp label86
		.align 64
		label86: jmp label87
		.align 64
		label87: jmp label88
		.align 64
		label88: jmp label89
		.align 64
		label89: jmp label90
		.align 64
		label90: jmp label91
		.align 64
		label91: jmp label92
		.align 64
		label92: jmp label93
		.align 64
		label93: jmp label94
		.align 64
		label94: jmp label95
		.align 64
		label95: jmp label96
		.align 64
		label96: jmp label97
		.align 64
		label97: jmp label98
		.align 64
		label98: jmp label99
		.align 64
		label99: jmp label100
		.align 64
		label100: jmp label101
		.align 64
		label101: jmp label102
		.align 64
		label102: jmp label103
		.align 64
		label103: jmp label104
		.align 64
		label104: jmp label105
		.align 64
		label105: jmp label106
		.align 64
		label106: jmp label107
		.align 64
		label107: jmp label108
		.align 64
		label108: jmp label109
		.align 64
		label109: jmp label110
		.align 64
		label110: jmp label111
		.align 64
		label111: jmp label112
		.align 64
		label112: jmp label113
		.align 64
		label113: jmp label114
		.align 64
		label114: jmp label115
		.align 64
		label115: jmp label116
		.align 64
		label116: jmp label117
		.align 64
		label117: jmp label118
		.align 64
		label118: jmp label119
		.align 64
		label119: jmp label120
		.align 64
		label120: jmp label121
		.align 64
		label121: jmp label122
		.align 64
		label122: jmp label123
		.align 64
		label123: jmp label124
		.align 64
		label124: jmp label125
		.align 64
		label125: jmp label126
		.align 64
		label126: jmp label127
		.align 64
		label127: jmp label128
		.align 64
		label128: jmp label129
		.align 64
		label129: jmp label130
		.align 64
		label130: jmp label131
		.align 64
		label131: jmp label132
		.align 64
		label132: jmp label133
		.align 64
		label133: jmp label134
		.align 64
		label134: jmp label135
		.align 64
		label135: jmp label136
		.align 64
		label136: jmp label137
		.align 64
		label137: jmp label138
		.align 64
		label138: jmp label139
		.align 64
		label139: jmp label140
		.align 64
		label140: jmp label141
		.align 64
		label141: jmp label142
		.align 64
		label142: jmp label143
		.align 64
		label143: jmp label144
		.align 64
		label144: jmp label145
		.align 64
		label145: jmp label146
		.align 64
		label146: jmp label147
		.align 64
		label147: jmp label148
		.align 64
		label148: jmp label149
		.align 64
		label149: jmp label150
		.align 64
		label150: jmp label151
		.align 64
		label151: jmp label152
		.align 64
		label152: jmp label153
		.align 64
		label153: jmp label154
		.align 64
		label154: jmp label155
		.align 64
		label155: jmp label156
		.align 64
		label156: jmp label157
		.align 64
		label157: jmp label158
		.align 64
		label158: jmp label159
		.align 64
		label159: jmp label160
		.align 64
		label160: jmp label161
		.align 64
		label161: jmp label162
		.align 64
		label162: jmp label163
		.align 64
		label163: jmp label164
		.align 64
		label164: jmp label165
		.align 64
		label165: jmp label166
		.align 64
		label166: jmp label167
		.align 64
		label167: jmp label168
		.align 64
		label168: jmp label169
		.align 64
		label169: jmp label170
		.align 64
		label170: jmp label171
		.align 64
		label171: jmp label172
		.align 64
		label172: jmp label173
		.align 64
		label173: jmp label174
		.align 64
		label174: jmp label175
		.align 64
		label175: jmp label176
		.align 64
		label176: jmp label177
		.align 64
		label177: jmp label178
		.align 64
		label178: jmp label179
		.align 64
		label179: jmp label180
		.align 64
		label180: jmp label181
		.align 64
		label181: jmp label182
		.align 64
		label182: jmp label183
		.align 64
		label183: jmp label184
		.align 64
		label184: jmp label185
		.align 64
		label185: jmp label186
		.align 64
		label186: jmp label187
		.align 64
		label187: jmp label188
		.align 64
		label188: jmp label189
		.align 64
		label189: jmp label190
		.align 64
		label190: jmp label191
		.align 64
		label191: jmp label192
		.align 64
		label192: jmp label193
		.align 64
		label193: jmp label194
		.align 64
		label194: jmp label195
		.align 64
		label195: jmp label196
		.align 64
		label196: jmp label197
		.align 64
		label197: jmp label198
		.align 64
		label198: jmp label199
		.align 64
		label199: jmp label200
		.align 64
		label200: jmp label201
		.align 64
		label201: jmp label202
		.align 64
		label202: jmp label203
		.align 64
		label203: jmp label204
		.align 64
		label204: jmp label205
		.align 64
		label205: jmp label206
		.align 64
		label206: jmp label207
		.align 64
		label207: jmp label208
		.align 64
		label208: jmp label209
		.align 64
		label209: jmp label210
		.align 64
		label210: jmp label211
		.align 64
		label211: jmp label212
		.align 64
		label212: jmp label213
		.align 64
		label213: jmp label214
		.align 64
		label214: jmp label215
		.align 64
		label215: jmp label216
		.align 64
		label216: jmp label217
		.align 64
		label217: jmp label218
		.align 64
		label218: jmp label219
		.align 64
		label219: jmp label220
		.align 64
		label220: jmp label221
		.align 64
		label221: jmp label222
		.align 64
		label222: jmp label223
		.align 64
		label223: jmp label224
		.align 64
		label224: jmp label225
		.align 64
		label225: jmp label226
		.align 64
		label226: jmp label227
		.align 64
		label227: jmp label228
		.align 64
		label228: jmp label229
		.align 64
		label229: jmp label230
		.align 64
		label230: jmp label231
		.align 64
		label231: jmp label232
		.align 64
		label232: jmp label233
		.align 64
		label233: jmp label234
		.align 64
		label234: jmp label235
		.align 64
		label235: jmp label236
		.align 64
		label236: jmp label237
		.align 64
		label237: jmp label238
		.align 64
		label238: jmp label239
		.align 64
		label239: jmp label240
		.align 64
		label240: jmp label241
		.align 64
		label241: jmp label242
		.align 64
		label242: jmp label243
		.align 64
		label243: jmp label244
		.align 64
		label244: jmp label245
		.align 64
		label245: jmp label246
		.align 64
		label246: jmp label247
		.align 64
		label247: jmp label248
		.align 64
		label248: jmp label249
		.align 64
		label249: jmp label250
		.align 64
		label250: jmp label251
		.align 64
		label251: jmp label252
		.align 64
		label252: jmp label253
		.align 64
		label253: jmp label254
		.align 64
		label254: jmp label255
		.align 64
		label255: jmp label256
		.align 64
		label256: jmp label257
		.align 64
		label257: jmp label258
		.align 64
		label258: jmp label259
		.align 64
		label259: jmp label260
		.align 64
		label260: jmp label261
		.align 64
		label261: jmp label262
		.align 64
		label262: jmp label263
		.align 64
		label263: jmp label264
		.align 64
		label264: jmp label265
		.align 64
		label265: jmp label266
		.align 64
		label266: jmp label267
		.align 64
		label267: jmp label268
		.align 64
		label268: jmp label269
		.align 64
		label269: jmp label270
		.align 64
		label270: jmp label271
		.align 64
		label271: jmp label272
		.align 64
		label272: jmp label273
		.align 64
		label273: jmp label274
		.align 64
		label274: jmp label275
		.align 64
		label275: jmp label276
		.align 64
		label276: jmp label277
		.align 64
		label277: jmp label278
		.align 64
		label278: jmp label279
		.align 64
		label279: jmp label280
		.align 64
		label280: jmp label281
		.align 64
		label281: jmp label282
		.align 64
		label282: jmp label283
		.align 64
		label283: jmp label284
		.align 64
		label284: jmp label285
		.align 64
		label285: jmp label286
		.align 64
		label286: jmp label287
		.align 64
		label287: jmp label288
		.align 64
		label288: jmp label289
		.align 64
		label289: jmp label290
		.align 64
		label290: jmp label291
		.align 64
		label291: jmp label292
		.align 64
		label292: jmp label293
		.align 64
		label293: jmp label294
		.align 64
		label294: jmp label295
		.align 64
		label295: jmp label296
		.align 64
		label296: jmp label297
		.align 64
		label297: jmp label298
		.align 64
		label298: jmp label299
		.align 64
		label299: jmp label300
		.align 64
		label300: jmp label301
		.align 64
		label301: jmp label302
		.align 64
		label302: jmp label303
		.align 64
		label303: jmp label304
		.align 64
		label304: jmp label305
		.align 64
		label305: jmp label306
		.align 64
		label306: jmp label307
		.align 64
		label307: jmp label308
		.align 64
		label308: jmp label309
		.align 64
		label309: jmp label310
		.align 64
		label310: jmp label311
		.align 64
		label311: jmp label312
		.align 64
		label312: jmp label313
		.align 64
		label313: jmp label314
		.align 64
		label314: jmp label315
		.align 64
		label315: jmp label316
		.align 64
		label316: jmp label317
		.align 64
		label317: jmp label318
		.align 64
		label318: jmp label319
		.align 64
		label319: jmp label320
		.align 64
		label320: jmp label321
		.align 64
		label321: jmp label322
		.align 64
		label322: jmp label323
		.align 64
		label323: jmp label324
		.align 64
		label324: jmp label325
		.align 64
		label325: jmp label326
		.align 64
		label326: jmp label327
		.align 64
		label327: jmp label328
		.align 64
		label328: jmp label329
		.align 64
		label329: jmp label330
		.align 64
		label330: jmp label331
		.align 64
		label331: jmp label332
		.align 64
		label332: jmp label333
		.align 64
		label333: jmp label334
		.align 64
		label334: jmp label335
		.align 64
		label335: jmp label336
		.align 64
		label336: jmp label337
		.align 64
		label337: jmp label338
		.align 64
		label338: jmp label339
		.align 64
		label339: jmp label340
		.align 64
		label340: jmp label341
		.align 64
		label341: jmp label342
		.align 64
		label342: jmp label343
		.align 64
		label343: jmp label344
		.align 64
		label344: jmp label345
		.align 64
		label345: jmp label346
		.align 64
		label346: jmp label347
		.align 64
		label347: jmp label348
		.align 64
		label348: jmp label349
		.align 64
		label349: jmp label350
		.align 64
		label350: jmp label351
		.align 64
		label351: jmp label352
		.align 64
		label352: jmp label353
		.align 64
		label353: jmp label354
		.align 64
		label354: jmp label355
		.align 64
		label355: jmp label356
		.align 64
		label356: jmp label357
		.align 64
		label357: jmp label358
		.align 64
		label358: jmp label359
		.align 64
		label359: jmp label360
		.align 64
		label360: jmp label361
		.align 64
		label361: jmp label362
		.align 64
		label362: jmp label363
		.align 64
		label363: jmp label364
		.align 64
		label364: jmp label365
		.align 64
		label365: jmp label366
		.align 64
		label366: jmp label367
		.align 64
		label367: jmp label368
		.align 64
		label368: jmp label369
		.align 64
		label369: jmp label370
		.align 64
		label370: jmp label371
		.align 64
		label371: jmp label372
		.align 64
		label372: jmp label373
		.align 64
		label373: jmp label374
		.align 64
		label374: jmp label375
		.align 64
		label375: jmp label376
		.align 64
		label376: jmp label377
		.align 64
		label377: jmp label378
		.align 64
		label378: jmp label379
		.align 64
		label379: jmp label380
		.align 64
		label380: jmp label381
		.align 64
		label381: jmp label382
		.align 64
		label382: jmp label383
		.align 64
		label383: jmp label384
		.align 64
		label384: jmp label385
		.align 64
		label385: jmp label386
		.align 64
		label386: jmp label387
		.align 64
		label387: jmp label388
		.align 64
		label388: jmp label389
		.align 64
		label389: jmp label390
		.align 64
		label390: jmp label391
		.align 64
		label391: jmp label392
		.align 64
		label392: jmp label393
		.align 64
		label393: jmp label394
		.align 64
		label394: jmp label395
		.align 64
		label395: jmp label396
		.align 64
		label396: jmp label397
		.align 64
		label397: jmp label398
		.align 64
		label398: jmp label399
		.align 64
		label399: jmp label400
		.align 64
		label400: jmp label401
		.align 64
		label401: jmp label402
		.align 64
		label402: jmp label403
		.align 64
		label403: jmp label404
		.align 64
		label404: jmp label405
		.align 64
		label405: jmp label406
		.align 64
		label406: jmp label407
		.align 64
		label407: jmp label408
		.align 64
		label408: jmp label409
		.align 64
		label409: jmp label410
		.align 64
		label410: jmp label411
		.align 64
		label411: jmp label412
		.align 64
		label412: jmp label413
		.align 64
		label413: jmp label414
		.align 64
		label414: jmp label415
		.align 64
		label415: jmp label416
		.align 64
		label416: jmp label417
		.align 64
		label417: jmp label418
		.align 64
		label418: jmp label419
		.align 64
		label419: jmp label420
		.align 64
		label420: jmp label421
		.align 64
		label421: jmp label422
		.align 64
		label422: jmp label423
		.align 64
		label423: jmp label424
		.align 64
		label424: jmp label425
		.align 64
		label425: jmp label426
		.align 64
		label426: jmp label427
		.align 64
		label427: jmp label428
		.align 64
		label428: jmp label429
		.align 64
		label429: jmp label430
		.align 64
		label430: jmp label431
		.align 64
		label431: jmp label432
		.align 64
		label432: jmp label433
		.align 64
		label433: jmp label434
		.align 64
		label434: jmp label435
		.align 64
		label435: jmp label436
		.align 64
		label436: jmp label437
		.align 64
		label437: jmp label438
		.align 64
		label438: jmp label439
		.align 64
		label439: jmp label440
		.align 64
		label440: jmp label441
		.align 64
		label441: jmp label442
		.align 64
		label442: jmp label443
		.align 64
		label443: jmp label444
		.align 64
		label444: jmp label445
		.align 64
		label445: jmp label446
		.align 64
		label446: jmp label447
		.align 64
		label447: jmp label448
		.align 64
		label448: jmp label449
		.align 64
		label449: jmp label450
		.align 64
		label450: jmp label451
		.align 64
		label451: jmp label452
		.align 64
		label452: jmp label453
		.align 64
		label453: jmp label454
		.align 64
		label454: jmp label455
		.align 64
		label455: jmp label456
		.align 64
		label456: jmp label457
		.align 64
		label457: jmp label458
		.align 64
		label458: jmp label459
		.align 64
		label459: jmp label460
		.align 64
		label460: jmp label461
		.align 64
		label461: jmp label462
		.align 64
		label462: jmp label463
		.align 64
		label463: jmp label464
		.align 64
		label464: jmp label465
		.align 64
		label465: jmp label466
		.align 64
		label466: jmp label467
		.align 64
		label467: jmp label468
		.align 64
		label468: jmp label469
		.align 64
		label469: jmp label470
		.align 64
		label470: jmp label471
		.align 64
		label471: jmp label472
		.align 64
		label472: jmp label473
		.align 64
		label473: jmp label474
		.align 64
		label474: jmp label475
		.align 64
		label475: jmp label476
		.align 64
		label476: jmp label477
		.align 64
		label477: jmp label478
		.align 64
		label478: jmp label479
		.align 64
		label479: jmp label480
		.align 64
		label480: jmp label481
		.align 64
		label481: jmp label482
		.align 64
		label482: jmp label483
		.align 64
		label483: jmp label484
		.align 64
		label484: jmp label485
		.align 64
		label485: jmp label486
		.align 64
		label486: jmp label487
		.align 64
		label487: jmp label488
		.align 64
		label488: jmp label489
		.align 64
		label489: jmp label490
		.align 64
		label490: jmp label491
		.align 64
		label491: jmp label492
		.align 64
		label492: jmp label493
		.align 64
		label493: jmp label494
		.align 64
		label494: jmp label495
		.align 64
		label495: jmp label496
		.align 64
		label496: jmp label497
		.align 64
		label497: jmp label498
		.align 64
		label498: jmp label499
		.align 64
		label499: jmp label500
		.align 64
		label500: jmp label501
		.align 64
		label501: jmp label502
		.align 64
		label502: jmp label503
		.align 64
		label503: jmp label504
		.align 64
		label504: jmp label505
		.align 64
		label505: jmp label506
		.align 64
		label506: jmp label507
		.align 64
		label507: jmp label508
		.align 64
		label508: jmp label509
		.align 64
		label509: jmp label510
		.align 64
		label510: jmp label511
		.align 64
		label511: jmp label512
		.align 64
		label512: xor %%eax, %%eax)"
		:
		:
		: "eax", "memory");
}