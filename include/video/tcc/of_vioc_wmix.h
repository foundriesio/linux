
#ifndef __OF_VIOC_WMIX_H__
#define	__OF_VIOC_WMIX_H__

struct vioc_wmix_data {
	unsigned int		width;
	unsigned int		height;
	unsigned int		layer;
	unsigned int		region;
	unsigned int		acon0;
	unsigned int		acon1;
	unsigned int		ccon0;
	unsigned int		ccon1;
	unsigned int		mode;
	unsigned int		asel;
	unsigned int		alpha0;
	unsigned int		alpha1;
};

struct vioc_wmix_pos {
	unsigned int		layer;
	unsigned int		sx;
	unsigned int		sy;
};

struct vioc_wmix_device {
	struct vioc_intr_type	*intr;
	void __iomem		*reg;
	void __iomem		*rst_reg;
	int			id;
	int			irq;
	unsigned int		path;
	unsigned int		mixmode;
	union{
		struct vioc_wmix_data		data;
		struct vioc_wmix_pos 		pos;
	};

};

extern void vioc_wmix_set_position(struct vioc_wmix_device *wmix, unsigned int update);
extern void vioc_wmix_set_image(struct vioc_wmix_device *wmix, unsigned int en);
extern void vioc_wmix_swreset(struct vioc_wmix_device *wmix, int reset);
extern int get_count_vioc_wmix(struct device *dev);
extern struct vioc_wmix_device *devm_vioc_wmix_get(struct device *dev, int index);

#endif
