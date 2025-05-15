SECTIONS
{
  .shared :
  {
    . = ALIGN(4096);
    shared_section_start = .; /* symbol at data start */
    *(.shared)                /* .shared section */
    . = ALIGN(4096);
    shared_section_end = .;   /* symbol at data end */
  }
}
INSERT AFTER .bss;
