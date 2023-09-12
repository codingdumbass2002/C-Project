#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/random.h>


uint8_t hd_mem[4194304];
uint8_t ra_mem[65536];

// This COUNT will be used later
int COUNT = 0;




struct seitentabellen_zeile {
    uint8_t present_bit;
    uint8_t dirty_bit;
    int8_t page_frame;
}seitentabelle[1024]; // 4194304 >> 12 = 1024



uint16_t get_seiten_nr(uint32_t virt_address) {

    return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
    // Extract the lower 12 bits (offset) from the virtual address
    uint16_t offset = virt_address & 0xFFF;

    // Calculate the page number by shifting the upper 20 bits of the virtual address
    uint32_t page_num = get_seiten_nr(virt_address);

    // Calculate the physical address by combining the page frame (shifted left by 12 bits)
    // with the offset using bitwise OR
    uint16_t physical_address = (seitentabelle[page_num].page_frame << 12) + offset;

    // Return the resulting physical address
    return physical_address;
}

int8_t check_present(uint32_t virt_address) {
    // If its present it will return 1 if not it will return 0
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}



int8_t is_mem_full() {
    // We can use a Brute force Method for simplicity (not Optimal), by just iterating through every page to check if they are all occupied
    for (int i = 0; i < 1024; i++) {
        if (!seitentabelle[i].present_bit) {

            // If one of the pages is not full it will return a 0 which means RAM is still not full
            return 0;
        }
    }
    return 1;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) {
    // This for loop will basically write the page from RAM to its designated place in Hard Drive
    // As we know the Offset will stay the same in both spaces so we need to keep track of it
    for (int offset = 0; offset < 4096; offset++){
        hd_mem[get_seiten_nr(virt_address) * 4096 + offset] = ra_mem[seitennummer * 4096 + offset];
    }
    return 1;
}

uint16_t swap_page(uint32_t virt_address) {
    // We first need to extract the Page Number of the Physical Adress by doing so:
    uint32_t page_number = virt_2_ram_address(virt_address) >> 12;

    // We now use the write page to hd function to write the page on HD
    write_page_to_hd(page_number, virt_address);

    // After that we switch it to "not dirty" since we just switched it which makes it clean
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;

    return 1;
}
//I used a not so optimal strategy for the sake of simplicity, this is in no way optimal!!
int8_t get_page_from_hd(uint32_t virt_address) {
    // This part of the function will be active when RAM is full:

    // Check if RAM is full
    if (is_mem_full()) {
        // RAM is full, so we'll replace page 1
        // Mark page 1 as not present in RAM
        seitentabelle[1].present_bit = 0;

        // Swap out page 1 from RAM to the hard disk
        swap_page(1 * 4096);

        // Copy the page from the hard disk to RAM
        for (int offset = 0; offset < 4096; offset++) {
            // Copy each byte of the page from hard disk to RAM
            ra_mem[1 * 4096 + offset] = hd_mem[get_seiten_nr(virt_address) * 4096 + offset];
        }

        // Mark the page as not dirty (unchanged)
        seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;
        // Update the page table entry for the new page in RAM
        seitentabelle[get_seiten_nr(virt_address)].page_frame = 1;
        // Mark the page as present in RAM
        seitentabelle[get_seiten_nr(virt_address)].present_bit = 1;

        // Return 1 to indicate success, and also to leave the function
        return 1;
    }

    //-------------------------------------------------------------------------------------------------------

    // In the case that its not full, we need to fill it up using the BB_COUNTER global variable
    else {
        for (int i = 0; i < 1024; i++) {
            // Check if the page is in RAM (present_bit == 1) and has the same page_frame as 'COUNT'
            if (seitentabelle[i].page_frame == COUNT && seitentabelle[i].present_bit == 1) {

                // Mark the page as not present in RAM
                seitentabelle[i].present_bit = 0;

                // Swap out the page from RAM to the hard disk, then break since we found the Page
                swap_page(i * 4096);
                break;

            }
        }
    }

    // Copy the page from the hard disk to RAM
    for (int offset = 0; offset < 4096; offset++) {
        // Copy each byte of the page from hard disk to RAM
        ra_mem[COUNT * 4096 + offset] = hd_mem[get_seiten_nr(virt_address) * 4096 + offset];
    }

    // Mark the page as not dirty (unchanged)
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 0;
    // Update the page table entry for the new page in RAM
    seitentabelle[get_seiten_nr(virt_address)].page_frame = COUNT;
    // Mark the page as present in RAM
    seitentabelle[get_seiten_nr(virt_address)].present_bit = 1;

    // Cycle to the next Page
    COUNT++;

    // we only have 16 Page frames so we dont want to go overboard
    if (COUNT == 16) {
        COUNT--;
    }

    // Return 1 to indicate success
    return 1;
}




uint8_t get_data(uint32_t virt_address) {
    // in the case that the requested data is not present in RAM we need to fetch it first from the hard drive
    if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }

    // Return the requested Data
    return ra_mem[virt_2_ram_address(virt_address)];
}

void set_data(uint32_t virt_address, uint8_t value) {

    // Same as the get_data function, we need to verify first that the data is present, if not we we get it from hard drive
    if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }
    // When it is present all we have to do is set the new value to it
    ra_mem[virt_2_ram_address(virt_address)] = value;

    // Ofcourse when we change something, in RAM, that means its now dirty therefore we mark it as dirty
    seitentabelle[get_seiten_nr(virt_address)].dirty_bit = 1;

}


int main(void) {
    puts("test driver_");
    static uint8_t hd_mem_expected[4194304];
    srand(1);
    fflush(stdout);
    for(int i = 0; i < 4194304; i++) {
        //printf("%d\n",i);
        uint8_t val = (uint8_t)rand();
        hd_mem[i] = val;
        hd_mem_expected[i] = val;
    }

    for (uint32_t i = 0; i < 1024;i++) {
//		printf("%d\n",i);
        seitentabelle[i].dirty_bit = 0;
        seitentabelle[i].page_frame = -1;
        seitentabelle[i].present_bit = 0;
    }


    uint32_t zufallsadresse = 4192425;
    uint8_t value = get_data(zufallsadresse);
//	printf("value: %d\n", value);

    if(hd_mem[zufallsadresse] != value) {
        printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
    }

    value = get_data(zufallsadresse);

    if(hd_mem[zufallsadresse] != value) {
        printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);

    }

//		printf("Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);


    srand(3);

    for (uint32_t i = 0; i <= 1000;i++) {
        uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if(hd_mem[zufallsadresse] != value) {
            printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1024;i++) {
                //printf("%d,%d-",i,seitentabelle[i].present_bit);
                if(seitentabelle[i].present_bit) {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }
            exit(1);
        }
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
        fflush(stdout);
    }


    srand(3);

    for (uint32_t i = 0; i <= 100;i++) {
        uint32_t zufallsadresse = rand() % 4095 *7;
        uint8_t value = (uint8_t)zufallsadresse >> 1;
        set_data(zufallsadresse, value);
        hd_mem_expected[zufallsadresse] = value;
//		printf("i : %d set_data address: %d - %d value at ram: %d\n",i,zufallsadresse,(uint8_t)value, ra_mem[virt_2_ram_address(zufallsadresse)]);
    }



    srand(4);
    for (uint32_t i = 0; i <= 16;i++) {
        uint32_t zufallsadresse = rand() % 4194304;//i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if(hd_mem_expected[zufallsadresse] != value) {
//			printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1024;i++) {
                //printf("%d,%d-",i,seitentabelle[i].present_bit);
                if(seitentabelle[i].present_bit) {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }

            exit(2);
        }
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem[zufallsadresse]);
        fflush(stdout);
    }

    srand(3);
    for (uint32_t i = 0; i <= 2500;i++) {
        uint32_t zufallsadresse = rand() % (4095 *5);//i * 4095 + 1;//rand() % 4194303
        uint8_t value = get_data(zufallsadresse);
        if(hd_mem_expected[zufallsadresse] != value ) {
            printf("ERROR_ at Address %d, Value %d =! %d!\n",zufallsadresse, hd_mem_expected[zufallsadresse], value);
            for (uint32_t i = 0; i <= 1024;i++) {
                //printf("%d,%d-",i,seitentabelle[i].present_bit);
                if(seitentabelle[i].present_bit) {
                    printf("i: %d, seitentabelle[i].page_frame %d\n", i, seitentabelle[i].page_frame);
                    fflush(stdout);
                }
            }
            exit(3);
        }
//		printf("i: %d data @ %u: %d hd value: %d\n",i,zufallsadresse, value, hd_mem_expected[zufallsadresse]);
        fflush(stdout);
    }

    puts("test end");
    fflush(stdout);
    return EXIT_SUCCESS;
}
