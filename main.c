#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/random.h>


uint8_t hd_mem[4194304];
uint8_t ra_mem[65536];

 uint32_t fifo_queue[1024];
 uint32_t fifo_front = 0;
 uint32_t fifo_rear = 0;


struct seitentabellen_zeile {
    uint8_t present_bit;
    uint8_t dirty_bit;
    int8_t page_frame;
}seitentabelle[1024]; // 4194304 >> 12 = 1024

uint16_t get_seiten_nr(uint32_t virt_address) {
    /**
     *
     */
    return virt_address >> 12;
}

uint16_t virt_2_ram_address(uint32_t virt_address) {
    // Extract the offset (lower 12 bits) from the virtual address
    uint16_t offset = virt_address & 0xFFF;

    // Extract the upper 20 bits from the virtual address
    uint32_t page_num = get_seiten_nr(virt_address);

    uint16_t physical_address = seitentabelle[page_num].page_frame << 12 | offset;

    return physical_address;
}

int8_t check_present(uint32_t virt_address) {
    /**
     * Wenn eine Seite im Arbeitsspeicher ist, gibt die Funktion "check_present" 1 zurück, sonst 0
     */
    return seitentabelle[get_seiten_nr(virt_address)].present_bit;
}

int8_t is_mem_full() {
    for (int i = 0; i < 1024; i++) {
        if (!seitentabelle[i].present_bit) {
            return 0;
        }
    }
    return 1;
}

int8_t write_page_to_hd(uint32_t seitennummer, uint32_t virt_address) { // alte addresse! nicht die neue!
    // Check if the page is dirty (needs to be written back)
    if (seitentabelle[seitennummer].dirty_bit) {
        for (int offset = 0; offset < 4096; offset++) {
            hd_mem[seitennummer * 4096 + offset] = ra_mem[offset];
        }

        // Mark the page as not dirty
        seitentabelle[seitennummer].dirty_bit = 0;

        return 1; // Successfully wrote page back to hard disk
    }

    return 0; // Page was not dirty, no write needed
}

uint16_t swap_page(uint32_t virt_address) {

    uint16_t swapped_page_frame = seitentabelle[fifo_front].page_frame;

    if (seitentabelle[fifo_front].dirty_bit) {
        write_page_to_hd(fifo_front, virt_address); // Write page to HD if dirty
    }

    // Reset page attributes for swapping
    seitentabelle[fifo_front].page_frame = -1;
    seitentabelle[fifo_front].present_bit = 0;

    // Update the front of the queue (FIFO replacement)
    fifo_front = (fifo_front + 1) % 1024;

    return swapped_page_frame; // Return the physical address of swapped-out page
}

int8_t get_page_from_hd(uint32_t virt_address) {
    uint32_t seitennummer = get_seiten_nr(virt_address);
    int8_t page_loaded = 0;

    if (is_mem_full()) {
        seitentabelle[seitennummer].page_frame = swap_page(virt_address);
        page_loaded = 1;
    } else {
        // Find the first free page frame
        for (uint32_t i = 0; i < 1024; i++) {
            if (seitentabelle[i].page_frame == -1) {
                seitentabelle[seitennummer].page_frame = i;
                page_loaded = 1;
                break;
            }
        }
    }

    if (page_loaded) {
        // Load the page from the hard disk to RAM
        for (int offset = 0; offset < 4096; offset++) {
            ra_mem[offset] = hd_mem[virt_address + offset];
        }

        // Update the page table entry to mark the page as present in RAM
        seitentabelle[seitennummer].present_bit = 1;
        return 1; // Successfully loaded page from hard disk
    }

    return 0;
}

uint8_t get_data(uint32_t virt_address) {
    uint16_t physical_address = virt_2_ram_address(virt_address);

    if (!check_present(virt_address)) {

        get_page_from_hd(virt_address);
    }


    return ra_mem[physical_address];
}

void set_data(uint32_t virt_address, uint8_t value) {
    uint16_t physical_address = virt_2_ram_address(virt_address); // Get the physical address in RAM

    if (!check_present(virt_address)) {
        get_page_from_hd(virt_address);
    }

    ra_mem[physical_address] = value;

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
            for (uint32_t i = 0; i <= 1023;i++) {
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
            for (uint32_t i = 0; i <= 1023;i++) {
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
            for (uint32_t i = 0; i <= 1023;i++) {
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
